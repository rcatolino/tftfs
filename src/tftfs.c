#define FUSE_USE_VERSION 26

#include <assert.h>
#include <errno.h>
#include <fuse.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "http.h"
#include "tft.h"
#include "utils.h"
#include "websocket.h"

int translate_treecode(int code) {
  // The validity of any translation between an underlying network/http
  // error to a filesystem error is questionable, at best. These could be
  // widely misinterpreted by the user, eg returning EACCES on http/401 means
  // that we have to authenticate (typically via the http-auth option) to
  // access this file, but the user will probably understand that the perms
  // on this file prevent him from accessing it, which doesn't mean anything
  // on tft.
  // That said it's the only way I've found, short of defining our own tft-errno.h,
  // to convey the reason of the error.
  switch (code) {
    case -1:
      return -EHOSTUNREACH;
    case -2:
      return -EBADMSG;
    case 200:
      return 0;
    case 401:
      return -EACCES;
    case 404:
      return -ENOENT;
    case 500:
      return -EBADE;
  }
  return -EPROTO;
}

int tft_getattr(const char *path, struct stat * buf) {
  // TODO: cache the result for a few sec
  int ret;
  fuse_debug("tft_getattr called, stat : %p, pid : %d, thread id : %lu\n", buf, getpid(), pthread_self());
  // Prefill the stat struct with the values not set in the tree fs.
  buf->st_nlink = 1; // No hardlinking on tft IIRC.
  buf->st_uid = getuid(); // No owner either (so far).
  buf->st_gid = getgid(); // No group concept either.
  buf->st_blocks = 0; // It doesn't take any room on the local drive.

  // Perform a tree request to get the rest.
  ret = translate_treecode(tree_getattr(tft_handle->hpool, path, buf));
  if (ret != 0) {
    return ret;
  }

  if (buf->st_size < 0) {
    // This indicate that the file doesn't exist
    int ret = buf->st_size;
    buf->st_size = 0;
    return ret;
  }

  return 0;
}

int tft_create(const char *path, mode_t mode, struct fuse_file_info *info) {
  struct file_handle *fh;
  int result = 0;
  int ret = 0;
  fuse_debug("tft_create called\n");

  // The mode is ignored since tree can't handle any perms yet.
  ret = translate_treecode(tree_create(tft_handle->hpool, path, &result));
  if (ret != 0) {
    fuse_debug("mkdir connection failed with %d\n", ret);
    return ret;
  } else if (result != 0) {
    return -result;
  }

  // Let's allocate a file handle, but not the internal buffer as long
  // as the file is empty.
  fh = malloc(sizeof(struct file_handle));
  fh->buf = NULL;
  fh->size = 0;
  fh->error_code = 0;
  fh->path = path;
  memset(&fh->json, 0, sizeof(struct json_buf));
  info->fh = (uint64_t) fh;
  return 0;
}

int tft_mkdir(const char *path, mode_t mode) {
  int result = 0;
  // The mode is ignored since tree can't handle any perms yet.
  int ret = translate_treecode(tree_mkdir(tft_handle->hpool, path, &result));
  if (ret != 0) {
    fuse_debug("mkdir connection failed with %d\n", ret);
    return ret;
  }

  fuse_debug("mkdir connection ended with %d\n", result);
  return -result;
}

int tft_unlink(const char *path) {
  int result = 0;
  int ret = translate_treecode(tree_unlink(tft_handle->hpool, path, &result));
  if (ret != 0) {
    return ret;
  }

  return -result;
}

int tft_rmdir(const char *path) {
  return tft_unlink(path);
}

int tft_open(const char *path, struct fuse_file_info *info) {
  struct file_handle *fh;
  int ret = 0;
  fuse_debug("tft_open called\n");
  if (info->flags & O_WRONLY || info->flags & O_RDWR) {
    return -EROFS; // TODO: remove when we support writing.
  }

  fh = malloc(sizeof(struct file_handle));
  fh->buf = NULL;
  fh->size = 0;
  fh->error_code = 0;
  fh->path = path;
  memset(&fh->json, 0, sizeof(struct json_buf));
  ret = tree_load_file(tft_handle->hpool, path, fh);
  // This will load the whole file in memory. Maybe we should swap it beyond a
  // certain size.

  if (fh->error_code != 0) {
    int error = fh->error_code;
    free(fh);
    return -error;
  } else if (ret == 200) {
    info->fh = (uint64_t) fh;
    return 0;
  }

  // There was an error, free the file_handle :
  free(fh);
  return translate_treecode(ret);
}

int tft_read(const char *path, char *buf, size_t buf_size, off_t offset,
		         struct fuse_file_info *info) {
  struct file_handle *fh = (struct file_handle *)info->fh;
  int to_read = 0;
  int data_left = 0;
  if (buf == NULL || buf_size <= 0) {
    return -EINVAL;
  }

  fuse_debug("tft_read called\n");
  assert(fh);
  data_left = fh->size - offset;
  if (!fh->buf || data_left <= 0) {
    // Empty file or end-of-file
    return 0;
  }

  to_read = buf_size > data_left ? data_left : buf_size;
  fuse_debug("reading %d bytes\n", to_read);
  memcpy(buf, fh->buf, to_read);
  return to_read;
}

int tft_release(const char *path, struct fuse_file_info *info) {
  struct file_handle *fh = (struct file_handle *)info->fh;
  fuse_debug("tft_release called\n");
  assert(fh);
  if (fh) free(fh->buf);
  free(fh);
  return 0; // ignored
}

int tft_write(const char *path, const char *buf, size_t buf_size, off_t offset,
		          struct fuse_file_info *info) {
  fuse_debug("tft_write called\n");
  return -ENOTSUP;
}

int tft_flush(const char *path, struct fuse_file_info *info) {
  fuse_debug("tft_flush called\n");
  return 0;
}

int tft_opendir(const char *path, struct fuse_file_info *info) {
  fuse_debug("tft_opendir called, path : %s, info : %p\n", path, info);
  return 0;
}

int tft_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
			          struct fuse_file_info *info) {
  struct tree_dir data;
  int ret = 0;
  data.filler_callback = filler;
  data.entry_offset = offset;
  data.dirent_buf = buf;
  data.error = 0;
  data.req_path = path;
  memset(&data.json, 0, sizeof(struct json_buf));
  fuse_debug("tft_readdir called : %s\n", path);
  ret = tree_readdir(tft_handle->hpool, path, &data);

  switch (ret) {
    case -1:
      return -EHOSTUNREACH;
    case -2:
      return -EBADMSG;
    case 200:
      break;
    case 401:
      return -EACCES;
    case 404:
      return -ENOENT;
    case 500:
      return -EBADE;
  }

  if (data.error != 0) {
    return -data.error;
  }
  return 0;
}

int tft_releasedir(const char *path, struct fuse_file_info *info) {
  fuse_debug("tft_releasedir called : %s\n", path);
  return 0;
}

void *tft_init(struct fuse_conn_info *conn) {
  fuse_debug("tft_init called, trying a websocket upgrade\n");
  ws_close(ws_upgrade(tft_handle->hpool), tft_handle->hpool);
  return tft_handle;
}

void tft_destroy(void *buff) {
  fuse_debug("tft_destroy called\n");
  free_pool(tft_handle->hpool);
  free(tft_handle);
}

static struct fuse_operations callbacks = {
  .getattr = tft_getattr,
  //.readlink = tft_readlink,
  //.getdir = tft_getdir,
  //.mknod = tft_mknod,
  .mkdir = tft_mkdir,
  .unlink = tft_unlink,
  .rmdir = tft_rmdir,
  //.symlink = tft_symlink,
  //.rename = tft_rename,
  //.link = tft_link,
  //.chmod = tft_chmod,
  //.chown = tft_chown,
  //.truncate = tft_truncate,
  //.utime = tft_utime,
  .open = tft_open,
  .read = tft_read,
  .write = tft_write,
  //.statfs = tft_statfs,
  .flush = tft_flush,
  .release = tft_release,
  //.fsync = tft_fsync,
  //.setxattr = tft_setxattr,
  //.getxattr = tft_getxattr,
  //.listxattr = tft_listxattr,
  //.removexattr = tft_removexattr,
  .opendir = tft_opendir,
  .readdir = tft_readdir,
  .releasedir = tft_releasedir,
  //.fsyncdir = tft_fsyncdir,
  .init = tft_init,
  .destroy = tft_destroy,
  //.access = tft_access,
  .create = tft_create,
  //.ftruncate = tft_ftruncate,
  //.fgetattr = tft_fgetattr
};

int main(int argc, char *argv[]) {

  struct tft_data *private_data = malloc(sizeof(struct tft_data));
  private_data->log = log_init();
  private_data->hpool = http_init(&argc, &argv);
  if (!private_data->hpool) {
    printf("Error, could not connect to server at %s.\n", argv[1]);
    return 1;
  }

  if (!isdir(argv[2])) {
    printf("Error, %s is not a valid directory\n", argv[2]);
    return 2;
  }

  argv[1] = argv[0];
  argv++;
  argc--;
  return fuse_main(argc, argv, &callbacks, private_data);
}
