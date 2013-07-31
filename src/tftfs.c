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

int tft_getattr(const char *path, struct stat * buf) {
  // TODO: cache the result for a few sec
  int ret;
  fuse_debug("tft_getattr called, stat : %p, pid : %d, thread id : %lu\n", buf, getpid(), pthread_self());
  // Prefill the stat struct with the values not set in the tree fs.
  buf->st_nlink = 1; // No hardlinking on tft IIRC.
  buf->st_uid = getuid(); // No owner either (so far).
  buf->st_gid = getgid(); // No goup concept either.
  buf->st_blocks = 0; // It doesn't take any room on the local drive.

  // Perform a tree request to get the rest.
  ret = tree_getattr(tft_handle->hpool, path, buf);
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

  if (buf->st_size < 0) {
    // This indicate that the file doesn't exist
    buf->st_size = 0;
    return -ENOENT;
  }

  return 0;
}

int tft_open(const char *path, struct fuse_file_info *info) {
  fuse_debug("tft_open called\n");
  return 0;
}

int tft_read(const char *path, char *buf, size_t buf_size, off_t offset,
		         struct fuse_file_info *info) {
  fuse_debug("tft_read called\n");
  return 0;
}

int tft_write(const char *path, const char *buf, size_t buf_size, off_t offset,
		          struct fuse_file_info *info) {
  fuse_debug("tft_write called\n");
  return 0;
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
  data.buf = buf;
  data.error = 0;
  data.req_path = path;
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
  fuse_debug("tft_init called\n");
  return tft_handle;
}

void tft_destroy(void *buff) {
  fuse_debug("tft_destroy called\n");
}

static struct fuse_operations callbacks = {
  .getattr = tft_getattr,
  //.readlink = tft_readlink,
  //.getdir = tft_getdir,
  //.mknod = tft_mknod,
  //.mkdir = tft_mkdir,
  //.unlink = tft_unlink,
  //.rmdir = tft_rmdir,
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
  //.release = tft_release,
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
  //.create = tft_create,
  //.ftruncate = tft_ftruncate,
  //.fgetattr = tft_fgetattr
};

int main(int argc, char *argv[]) {

  struct tft_data *private_data = malloc(sizeof(struct tft_data));
  private_data->log = log_init();
  private_data->hpool = http_init(&argc, &argv);
  if (!private_data->hpool) {
    return 1;
  }

  if (!isdir(argv[2])) {
    printf("Error, %s is not a valid directory\n", argv[2]);
    return 2;
  }

  if (!private_data->hpool) {
    printf("Invalid url : %s.\n", argv[1]);
    return 3;
  }

  argv[1] = argv[0];
  argv++;
  argc--;
  debug("%d, %s %s\n", argc, argv[0], argv[1]);
  return fuse_main(argc, argv, &callbacks, private_data);
}
