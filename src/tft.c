#include <assert.h>
#include <jansson.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "connections.h"
#include "http.h"
#include "response_parsing.h"
#include "tft.h"
#include "utils.h"

#define TFT_READ_OP "op=ls&path=%s&depth=%d"
#define TFT_DELETE_OP "op=rm&path=%s"
#define TFT_CREATE_OP "op=new&path=%s&type=%s"

#define ALL_DONE (block_size*nbblock)

off_t get_file_size(struct http_connection *con) {
  double size = 0;
  curl_easy_getinfo(con->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &size);
  if (size == -1) {
    size = curl_easy_getinfo(con->curl, CURLINFO_SIZE_DOWNLOAD, &size);
  }

  fuse_debug("get_file_size : %ld\n", (off_t)size);
  return (off_t)size;
}

int tree_read(struct connection_pool *pool, const char *path, int depth,
              result_callback callback, void *userdata) {
  int data_size = strlen(path) + sizeof(TFT_READ_OP) + 5 - 4;
  long ret = 0;
  // The 5 is for the depth arg. Having more than a 5 digit depth would probably
  // break the world anyway. the -4 is to account for the format characters %?.
  char *post_data = malloc(data_size);
  memset(post_data, 0, data_size);
  if (snprintf(post_data, data_size, TFT_READ_OP, path, depth) >= data_size) {
    fuse_debug("Buffer error in tree_read()\n");
    assert(0);
  }

  struct http_connection *con = post(pool, "fs", post_data, callback, userdata);
  ret = http_get_resp_code(con);
  if (con->last_result.result == 23) {
    // There was an error in the parsing callback
    ret = -2;
  } else if (con->last_result.result != 0) {
    // Test the network layer result
    fuse_debug("Curl error : %d\n", con->last_result.result);
    ret = -1;
  } else if (ret != 200) {
    // Test the application layer result
    fuse_debug("HTTP Error %ld\n", ret);
  }

  release(pool, con);
  free(post_data);
  return (int)ret;
}

void fill_stat(struct stat *stbuf, const char *type, time_t mtime) {
  if (strncmp(type, "dir", 3) == 0) {
    stbuf->st_mode |= S_IFDIR;
  } else {
    stbuf->st_mode |= S_IFREG; // One day we might have to test for more.
    // TODO report the size as well!
    stbuf->st_size = 42;
  }

  stbuf->st_nlink = 1;
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_atime = mtime;
  stbuf->st_mtime = mtime;
  stbuf->st_ctime = mtime;
}

size_t getattr_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  struct stat *stbuf = (struct stat *)userdata;
  json_t *files;
  const char *type;
  time_t mtime;

  //fuse_debug("Answer from getattr request %ld bytes : %.*s\n", ALL_DONE, (int)ALL_DONE, buffer);
  if (ALL_DONE == 0) {
    return 0;
  }

  switch (load_response(buffer, ALL_DONE, &files, "getattr_callback")) {
    case -1:
      return 0;
    case 1:
      // No such file
      return ALL_DONE;
    case 0:
      break;
  }

  stbuf->st_mode = 0;
  stbuf->st_mode |= S_IRWXU | S_IRWXG | S_IRWXO; // There are no perms yet
  if (json_array_size(files) == 0) {
    fuse_debug("Error in getattr_callback, no files in the answer\n");
    goto err;
  } else if (json_array_size(files) > 1) {
    fuse_debug("Error in getattr_callback, there should be only one file in a getattr request.\n");
    goto err;
  }

  if (get_metadata(json_array_get(files, 0), &type, &mtime, "getattr_callback") == -1) {
    goto err;
  }

  fill_stat(stbuf, type, mtime);
  json_decref(files);
  return ALL_DONE;

err:
  json_decref(files);
  return 0;
}

size_t readdir_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  struct tree_dir *dir = (struct tree_dir *)userdata;
  json_t *files;

  fuse_debug("Answer from readdir request %ld bytes : %.*s\n", ALL_DONE, (int)ALL_DONE, buffer);
  if (ALL_DONE == 0) {
    return 0;
  }

  switch (load_response(buffer, ALL_DONE, &files, "readdir_callback")) {
    case -1:
      return 0;
    case 1:
      // No such file
      return ALL_DONE;
    case 0:
      break;
  }

  // Go through all the files and fill the dir buffer for each one.
  for (int i = 0; i < json_array_size(files); i++) {
    const char *path;
    const char *name;
    const char *type;
    time_t mtime;
    struct stat stbuf;
    memset(&stbuf, 0, sizeof(struct stat));

    if (get_path(json_array_get(files, i), &path, "readdir_callback") == -1) {
      goto err;
    } else if (get_metadata(json_array_get(files, 0), &type, &mtime, "getattr_callback") == -1) {
      goto err;
    } else if (strcmp(path, dir->req_path) == 0) {
      continue;
    }

    fill_stat(&stbuf, type, mtime);
    // Get the filename component of the path :
    name = strrchr(path, '/');
    name = name ? name + 1 : path;
    fuse_debug("readdir, direntry %s : %s\n", path, name);
    dir->filler_callback(dir->buf, name, &stbuf, 0);
  }

  json_decref(files);
  return ALL_DONE;

err:
  json_decref(files);
  return 0;
}

int tree_getattr(struct connection_pool *pool, const char *path, struct stat *buff) {
  int ret = tree_read(tft_handle->hpool, path, -1, getattr_callback, buff);
  return ret;
}

int tree_readdir(struct connection_pool *pool, const char *path, struct tree_dir *data) {
  int ret = tree_read(tft_handle->hpool, path, 0, readdir_callback, data);
  return ret;
}

