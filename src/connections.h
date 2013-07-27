#ifndef CONNECTIONS_H__
#define CONNECTIONS_H__

#include <curl/curl.h>
#include <pthread.h>

#define DEFAULT_RES_SIZE 10*1024
#define DEFAULT_POOL_SIZE 5

struct http_result {
  char * buffer;
};

struct http_connection {
  CURL * curl;
  char * root_url;
  struct http_result last_result;
};

struct connection_pool {
  struct http_connection **queue;
  int pool_size;
  int available_connections;
  pthread_cond_t connection_available;
  pthread_mutex_t cond_mutex;
  CURLSH * shared_connection;
};

struct connection_pool *create_pool(struct http_connection *initial);
void free_pool(struct connection_pool *pool);

struct http_connection *acquire_connection(struct connection_pool *pool);
void release_connection(struct http_connection *con, struct connection_pool *pool);

#endif
