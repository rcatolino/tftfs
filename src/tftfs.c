#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"

int tft_open(const char *path, struct fuse_file_info *info) {
  return 0;
}

int tft_read(const char *path, char *buf, size_t buf_size, off_t offset,
		         struct fuse_file_info *info) {
  return 0;
}

int tft_write(const char *path, const char *buf, size_t buf_size, off_t offset,
		          struct fuse_file_info *info) {
  return 0;
}

int tft_flush(const char *path, struct fuse_file_info *info) {
  return 0;
}

int tft_opendir(const char *path, struct fuse_file_info *info) {
  return 0;
}

int tft_readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset,
			          struct fuse_file_info *info) {
  return 0;
}

void *tft_init(struct fuse_conn_info *conn) {
  return NULL;
}

void tft_destroy(void *buff) {
}

static struct fuse_operations callbacks = {
  //.getattr = tft_getattr,
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
  //.releasedir = tft_releasedir,
  //.fsyncdir = tft_fsyncdir,
  .init = tft_init,
  .destroy = tft_destroy,
  //.access = tft_access,
  //.create = tft_create,
  //.ftruncate = tft_ftruncate,
  //.fgetattr = tft_fgetattr
};

int main(int argc, char *argv[]) {

  if (argc < 3) {
    printf("Error, you need to specify a tree url and a mount point on the local filesystem.%d\n",argc);
    return 1;
  } else if (!isdir(argv[2])) {
    printf("Error, %s is not a valid directory\n", argv[2]);
    return 2;
  }

  argv[1] = argv[0];
  argv++;
  argc--;
  debug("%d, %s %s\n", argc, argv[0], argv[1]);
  fuse_main(argc, argv, &callbacks, NULL);

  return 0;
}
