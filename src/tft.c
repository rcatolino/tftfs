#include <jansson.h>
#include <stdlib.h>
#include <string.h>

#include "connections.h"
#include "http.h"
#include "utils.h"

#define TFT_READ_OP "op=ls&path="
#define TFT_DELETE_OP "op=rm&path="
#define TFT_CREATE_OP "op=new&path="

struct tree_dir *tree_readdir(struct connection_pool *pool, const char *path) {
  // TODO:  Allocate the buffer on opendir, and free it on releasedir instead
  char *post_data = malloc(strlen(path) + sizeof(TFT_READ_OP));
  memset(post_data, 0, strlen(path) + sizeof(TFT_READ_OP));
  strcat(post_data, TFT_READ_OP);
  strcat(post_data, path);
  struct http_connection *con = post(pool, "fs", post_data);
  release(pool, con);
  free(post_data);
  return NULL;
}
