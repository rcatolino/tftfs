#ifndef HTTP_H__
#define HTTP_H__

#include <curl/curl.h>
#include <sys/stat.h>

#include "connections.h"

typedef size_t (*result_callback) (char *buffer, size_t block_size, size_t nbblock, void *userdata);

void http_cleanup(struct http_connection *data);

long http_get_resp_code(struct http_connection* con);

struct connection_pool *http_init(int * argc, char *(*argv[]));

void make_action_url(struct http_connection *con, const char *action);

struct http_connection *post(struct connection_pool *pool, const char *action, const char *data,
                             result_callback callback, void *userdata);

void release(struct connection_pool *pool, struct http_connection *con);
#endif
