#include <assert.h>
#include <errno.h>
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
#define TFT_CREATE_OP "op=new&type=%s&path=%s"

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

int tree_rm(struct connection_pool *pool, const char *path,
            result_callback callback, void *userdata) {
  int data_size = strlen(path) + sizeof(TFT_DELETE_OP) - 2;
  long ret = 0;
  char *post_data = malloc(data_size);
  memset(post_data, 0, data_size);
  if (snprintf(post_data, data_size, TFT_DELETE_OP, path) >= data_size) {
    fuse_debug("Buffer error in tree_rm()\n");
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

int tree_new(struct connection_pool *pool, const char *path, const char *type,
             result_callback callback, void *userdata) {
  int data_size = strlen(path) + strlen(type) + sizeof(TFT_CREATE_OP) - 4;
  long ret = 0;
  char *post_data = malloc(data_size);
  memset(post_data, 0, data_size);
  if (snprintf(post_data, data_size, TFT_CREATE_OP, type, path) >= data_size) {
    fuse_debug("Buffer error in tree_rm()\n");
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


void fill_stat(struct stat *stbuf, const char *type, time_t mtime, size_t size) {
  if (strncmp(type, "dir", 3) == 0) {
    stbuf->st_mode |= S_IFDIR;
  } else {
    stbuf->st_mode |= S_IFREG; // One day we might have to test for more.
  }

  stbuf->st_nlink = 1;
  stbuf->st_uid = getuid();
  stbuf->st_gid = getgid();
  stbuf->st_atime = mtime;
  stbuf->st_mtime = mtime;
  stbuf->st_ctime = mtime;
  stbuf->st_size = size;
}

size_t getattr_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  struct stat *stbuf = (struct stat *)userdata;
  size_t buf_size = block_size*nbblock;
  json_t *files;
  const char *type;
  time_t mtime;
  size_t size;
  int ret;

  //fuse_debug("Answer from getattr request %ld bytes : %.*s\n", buf_size, (int)buf_size, buffer);
  if (buf_size == 0) {
    return 0;
  }

  switch (ret = load_response(buffer, buf_size, &files, "getattr_callback")) {
    case -1:
      return 0;
    case -2:
      return 0; // We don't support incomplete answers
    case 0:
      break;
    default:
      // No such file
      stbuf->st_size = -ret;
      return buf_size;
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

  if (get_metadata(json_array_get(files, 0), &type, &mtime, &size, "getattr_callback") == -1) {
    goto err;
  }

  fill_stat(stbuf, type, mtime, size);
  json_decref(files);
  return buf_size;

err:
  json_decref(files);
  return 0;
}

static void free_chunks(struct json_buf *json) {
  if (json->buf) {
    free(json->buf); // The chunks stored won't be used because of the error.
    memset(json, 0, sizeof(struct json_buf));
  }
}

static void store_chunk(char *buffer, size_t buffer_size, struct json_buf *json) {
  size_t room_left = json->buf_size - json->size;
  fuse_debug("store_chunk : room left in json buffer : %ld, data size : %ld\n", room_left, buffer_size);
  if (room_left < buffer_size) {
    size_t new_size = (json->size + buffer_size) * 2;
    fuse_debug("store_chunk : (re-)allocating from %ld to %ld bytes\n", json->buf_size, new_size);
    json->buf = realloc(json->buf, new_size);
    assert(json->buf);
    json->buf_size = new_size;
  }

  memcpy(json->buf + json->size, buffer, buffer_size);
  json->size += buffer_size;
}

size_t readdir_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  struct tree_dir *dir = (struct tree_dir *)userdata;
  size_t buf_size = block_size*nbblock;
  json_t *files;
  int ret = 0;

  //fuse_debug("Answer from readdir request %ld bytes : %.*s\n", buf_size, (int)buf_size, buffer);
  if (buf_size == 0) {
    return 0;
  }

  if (dir->json.buf) {
    // We already started collecting the answer in a previous call
    store_chunk(buffer, buf_size, &dir->json);
    ret = load_response(dir->json.buf, dir->json.size, &files, "readdir_callback");
  } else {
    // No buff saved just yet, just use the received one
    ret = load_response(buffer, buf_size, &files, "readdir_callback");
    if (ret == -2) {
      // Incomplete json, store the received chunk.
      store_chunk(buffer, buf_size, &dir->json);
    }
  }

  if (ret > 0) {
    dir->error = ret;
    free_chunks(&dir->json); // The chunks stored won't be used because of the error.
    return buf_size;
  }

  switch (ret) {
    case -1:
      free_chunks(&dir->json); // The chunks stored won't be used because of the error.
      return 0;
    case -2:
      // Incomplete response, return.
      return buf_size;
    case 0:
      break;
  }

  // Go through all the files and fill the dir buffer for each one.
  for (int i = 0; i < json_array_size(files); i++) {
    const char *path;
    const char *name;
    const char *type;
    time_t mtime;
    size_t size;
    struct stat stbuf;
    memset(&stbuf, 0, sizeof(struct stat));

    if (get_path(json_array_get(files, i), &path, "readdir_callback") == -1) {
      goto err;
    } else if (get_metadata(json_array_get(files, 0), &type, &mtime, &size, "getattr_callback") == -1) {
      goto err;
    } else if (strcmp(path, dir->req_path) == 0) {
      continue;
    }

    fill_stat(&stbuf, type, mtime, size);
    // Get the filename component of the path :
    name = strrchr(path, '/');
    name = name ? name + 1 : path;
    //fuse_debug("readdir, direntry %s : %s\n", path, name);
    dir->filler_callback(dir->dirent_buf, name, &stbuf, 0);
  }

  json_decref(files);
  free_chunks(&dir->json); // We have treated every chunk successfully.
  return buf_size;

err:
  json_decref(files);
  free_chunks(&dir->json); // Error we can't process the json chunks.
  return 0;
}

size_t readcontent_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  struct file_handle *fh = (struct file_handle *)userdata;
  size_t buf_size = block_size*nbblock;
  json_t *files;
  const char *type;
  size_t size;
  const char *content;
  int ret = 0;

  //fuse_debug("Answer from load_file request %ld bytes : %.*s\n", buf_size, (int)buf_size, buffer);
  if (buf_size == 0) {
    return 0;
  }

  if (fh->json.buf) {
    // We already started collecting the answer in a previous call
    store_chunk(buffer, buf_size, &fh->json);
    ret = load_response(fh->json.buf, fh->json.size, &files, "readdir_callback");
  } else {
    // No buff saved just yet, just use the received one
    ret = load_response(buffer, buf_size, &files, "readdir_callback");
    if (ret == -2) {
      // Incomplete json, store the received chunk.
      store_chunk(buffer, buf_size, &fh->json);
    }
  }

  if (ret > 0) {
    fh->error_code = ret;
    free_chunks(&fh->json); // The chunks stored won't be used because of the error.
    return buf_size;
  }

  switch (ret) {
    case -1:
      free_chunks(&fh->json); // The chunks stored won't be used because of the error.
      return 0;
    case -2:
      // Incomplete response, return.
      return buf_size;
    case 0:
      break;
  }

  if (json_array_size(files) != 1) {
    fuse_debug("Error in readcontent_callback : wrong number of file in the answer : %ld",
               json_array_size(files));
  }

  // Get the type and content of the file.
  if(get_metadata(json_array_get(files, 0), &type, NULL, &size, "readcontent_callback") == -1 ||
     get_content(json_array_get(files, 0), &content, "readcontent_callback") == -1) {
    goto err;
  } else if (strcmp(type, "dir") == 0) {
    // We can't open a dir!
    fh->error_code = EISDIR;
    goto out;
  } else {
    // Copy the content of the file into the file handle buffer.
    assert(fh->buf == NULL);
    fh->size = strlen(content); // We don't want the trailing null byte
    if (fh->size != size) {
      fuse_log("Warning, %s has an actual size of %ld bytes that differs from the expected size %ld bytes\n",
               fh->path, fh->size, size);
    }
    fh->buf = malloc(fh->size);
    memcpy(fh->buf, content, fh->size);
  }

out:
  json_decref(files);
  free_chunks(&fh->json); // We have treated every chunk successfully.
  return buf_size;

err:
  json_decref(files);
  free_chunks(&fh->json); // Error we can't process the json chunks.
  return 0;
}

size_t unlink_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  int *ret = (int *)userdata;
  size_t buf_size = block_size*nbblock;
  json_t *result;
  int err;

  fuse_debug("Answer from unlink request %ld bytes : %.*s\n", buf_size, (int)buf_size, buffer);
  if (buf_size == 0) {
    return 0;
  }

  err = load_buffer(buffer, buf_size, &result, "unlink_callback");
  json_decref(result);  // We don't really care about the data, just about the error code.
  if (err > 0) {
    // The request wasn't valid.
    *ret = err;
  } else if (err == -1) {
    // The communication failed.
    *ret = -1;
    return 0;
  } else if (err == -2) {
    // Incomplete answer (this shouldn't happen often).
    *ret = EFBIG;
  } else {
    // Done.
    *ret = 0;
  }

  return buf_size;
}

size_t default_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  // This callback just check for errors, and returns them in the data buffer
  // wich should be able to hold an int
  int *ret = (int *)userdata;
  size_t buf_size = block_size*nbblock;
  json_t *result;
  int err;

  fuse_debug("Answer from default %ld bytes : %.*s\n", buf_size, (int)buf_size, buffer);
  if (buf_size == 0) {
    return 0;
  }

  err = load_buffer(buffer, buf_size, &result, "unlink_callback");
  json_decref(result);  // We don't really care about the data, just about the error code.
  if (err > 0) {
    // The request wasn't valid.
    *ret = err;
  } else if (err == -1) {
    // The communication failed.
    *ret = -1;
    return 0;
  } else if (err == -2) {
    // Incomplete answer (this shouldn't happen often).
    *ret = EFBIG;
  } else {
    // Done.
    *ret = 0;
  }

  return buf_size;
}

int tree_getattr(struct connection_pool *pool, const char *path, struct stat *buff) {
  int ret = tree_read(pool, path, -1, getattr_callback, buff);
  return ret;
}

int tree_mkdir(struct connection_pool *pool, const char *path, int *result) {
  int ret = tree_new(pool, path, "dir", default_callback, result);
  return ret;
}

int tree_create(struct connection_pool *pool, const char *path, int *result) {
  int ret = tree_new(pool, path, "", default_callback, result);
  return ret;
}

int tree_readdir(struct connection_pool *pool, const char *path, struct tree_dir *data) {
  int ret = tree_read(pool, path, 0, readdir_callback, data);
  return ret;
}

int tree_load_file(struct connection_pool *pool, const char *path, struct file_handle *fh) {
  int ret = tree_read(pool, path, 0, readcontent_callback, fh);
  return ret;
}

int tree_unlink(struct connection_pool *pool, const char *path, int *result) {
  int ret = tree_rm(pool, path, unlink_callback, result);
  return ret;
}


