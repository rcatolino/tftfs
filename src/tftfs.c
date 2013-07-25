#define FUSE_USE_VERSION 26
#include <fuse.h>
#include <glib.h>
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

  int opt_readonly = 0;
  char **rest;
  char *fuse_arg[argc];
  GError *error = NULL;
  GOptionContext *opt_context;
  GOptionEntry opt_entries[] = {
    { "read-only", 'r', 0, G_OPTION_ARG_NONE, &opt_readonly, "Mount read-only", NULL},
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_STRING_ARRAY, &rest,
      "The location of the tree, and the local mount point", "url mount-point"},
    { NULL }
  };

  opt_context = g_option_context_new("");
  g_option_context_set_summary(opt_context,
                               "Example: to mount http://thefiletree.com/example/ at ./tft use :\n"
                               "\n"
                               "  ./tftfs [-r] http://thefiletree.com/example/ tft");
  g_option_context_add_main_entries(opt_context, opt_entries, NULL);
  if (!g_option_context_parse(opt_context, &argc, &argv, &error)) {
    g_printerr ("Error parsing options: %s\n", error->message);
    goto parse_error;
  }

  if (opt_readonly) {
    debug("readonly\n");
    // Take care of readonly option
  }

  if (rest[0] == NULL || rest[1] == NULL) {
    printf("Error, you need to specify a tree url and a mount point on the local filesystem.%d\n",argc);
    goto arg_err;
  } else if (rest[2] != NULL) {
    printf("Bad argument : %s.\n", rest[2]);
    goto arg_err;
  }

  if (!isdir(rest[1])) {
    printf("Error, %s is not a valid directory\n", rest[1]);
    goto out;
  }

  fuse_arg[0] = argv[0];
  fuse_arg[1] = rest[1];
  fuse_main(2, fuse_arg, &callbacks, NULL);

out :
  g_strfreev(rest);
arg_err :
  g_option_context_free(opt_context);
parse_error :
  if (error) {
    g_error_free(error);
    return 1;
  }

  return 0;
}
