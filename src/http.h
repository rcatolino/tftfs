#ifndef HTTP_H__
#define HTTP_H__

#include <curl/curl.h>
#include <sys/stat.h>

#include "connections.h"

void http_cleanup(struct http_connection *data);
struct connection_pool *http_init(int * argc, char *(*argv[]));

struct http_connection *get(struct connection_pool *pool, const char *local_path);
struct http_connection *post(struct connection_pool *pool, const char *action, const char *data);
void release(struct connection_pool *pool, struct http_connection *con);
mode_t get_file_type(struct http_connection *con);
off_t get_file_size(struct http_connection *con);

#endif
