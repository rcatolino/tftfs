#ifndef CONNECTIONS_H__
#define CONNECTIONS_H__

#include <curl/curl.h>
#include <pthread.h>

#define DEFAULT_RES_SIZE 10*1024
#define DEFAULT_POOL_SIZE 5

struct http_result {
  char *effective_url;
  char *callback_userdata; // UNUSED SO FAR
  CURLcode result;
};

enum con_mode {
  REGULAR, WS
};

struct http_connection {
  CURL *curl;
  char *root_url; // Contains the whole url as passed in by the user,
                  // in the form '[scheme://]host/root_path'
  char *host; // Contains the '[scheme://]host' part of the root_url.
  char *root_path; // Contains the 'root_path' part of the url.
                   // The same buffer as root_url's is used.
  struct http_result last_result;
  enum con_mode mode;
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
