#ifndef TFT_H__
#define TFT_H__

#include <fuse.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "connections.h"

struct json_buf {
    void *buf;
    size_t size;
    size_t buf_size;
};

struct tree_dir {
  fuse_fill_dir_t filler_callback;
  off_t entry_offset;
  void *dirent_buf;
  const char *req_path;
  int error;
  struct json_buf json;
};

struct file_handle {
  char *buf;
  const char *path;
  size_t size;
  int error_code;
  struct json_buf json;
};

// Tree level operations
int tree_read(struct connection_pool *pool, const char *path, int depth,
              result_callback callback, void *userdata);
int tree_rm(struct connection_pool *pool, const char *path,
            result_callback callback, void *userdata);

// File system level operations
int tree_unlink(struct connection_pool *pool, const char *path, int *result);
int tree_getattr(struct connection_pool *pool, const char *path, struct stat *buff);
int tree_readdir(struct connection_pool *pool, const char *path, struct tree_dir *data);
int tree_load_file(struct connection_pool *pool, const char *path, struct file_handle *fh);

#endif
