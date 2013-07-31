#ifndef TFT_H__
#define TFT_H__

#include <fuse.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "connections.h"

struct tree_dir {
  fuse_fill_dir_t filler_callback;
  off_t entry_offset;
  void *buf;
  const char *req_path;
  int error;
};

int tree_read(struct connection_pool *pool, const char *path, int depth,
              result_callback callback, void *userdata);
int tree_getattr(struct connection_pool *pool, const char *path, struct stat *buff);
int tree_readdir(struct connection_pool *pool, const char *path, struct tree_dir *data);
mode_t get_file_type(struct http_connection *con);
off_t get_file_size(struct http_connection *con);

#endif
