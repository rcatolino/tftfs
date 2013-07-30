#ifndef TFT_H__
#define TFT_H__

#include "connections.h"

struct tree_dir {
  char **entries;
  int *last_access;
  int nb_entry;
};

struct tree_dir *tree_readdir(struct connection_pool *pool, const char *path);

#endif
