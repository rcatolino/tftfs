#include <assert.h>
#include <pthread.h>
#include <stdlib.h>

#include "connections.h"
#include "http.h"
#include "utils.h"

struct connection_pool *create_pool(struct http_connection *initial) {
  struct connection_pool *pool = malloc(sizeof(struct connection_pool));
  if (!pool) {
    perror("Error initializing connection pool ");
    goto error_malloc_pool;
  }

  pool->queue = malloc(DEFAULT_POOL_SIZE * sizeof(struct http_connection*));
  if (!pool->queue) {
    perror("Error initializing connection pool buffers");
    goto error_malloc_queue;
  }

  pool->available_connections = DEFAULT_POOL_SIZE;
  pool->pool_size = DEFAULT_POOL_SIZE;

  if (pthread_cond_init(&pool->connection_available, NULL) != 0) {
    perror("Error initializing connection pool condition ");
    goto error_cond;
  }

  if (pthread_mutex_init(&pool->cond_mutex, NULL) != 0) {
    perror("Error initializing connection pool synchronization ");
    goto error_mutex;
  }

  pool->queue[0] = initial;
  for (int i = 1; i<DEFAULT_POOL_SIZE; i++) {
    pool->queue[i] = malloc(sizeof(struct http_connection));
    pool->queue[i]->curl = curl_easy_duphandle(initial->curl);
    pool->queue[i]->root_url = initial->root_url;
    pool->queue[i]->last_result.buffer = malloc(DEFAULT_RES_SIZE);
  }

  return pool;

error_mutex:
  pthread_cond_destroy(&pool->connection_available);
error_cond:
  free(pool->queue);
error_malloc_queue:
  free(pool);
error_malloc_pool:
  return NULL;
}

void free_pool(struct connection_pool *pool) {
  for (int i = 0; i<pool->pool_size; i++) {
    if (!pool->queue[i]) {
      continue;
    }

    http_cleanup(pool->queue[i]);
  }

  curl_global_cleanup();
  pthread_cond_destroy(&pool->connection_available);
  pthread_mutex_destroy(&pool->cond_mutex);
  free(pool->queue);
  free(pool);
}

struct http_connection *acquire_connection(struct connection_pool *pool) {
  struct http_connection *con;

  pthread_mutex_lock(&pool->cond_mutex);
  while(pool->available_connections == 0) {
    pthread_cond_wait(&pool->connection_available, &pool->cond_mutex);
  }

  // Remove the last available con :
  con = pool->queue[pool->available_connections - 1];
  pool->queue[pool->available_connections - 1] = NULL;
  pool->available_connections -= 1;
  assert(con);

  pthread_mutex_unlock(&pool->cond_mutex);
  return con;
}

void release_connection(struct http_connection *con, struct connection_pool *pool) {
  pthread_mutex_lock(&pool->cond_mutex);

  // Move this connection to the pool
  assert(!pool->queue[pool->available_connections]);
  pool->queue[pool->available_connections] = con;
  pool->available_connections += 1;

  if (pool->available_connections == 1) {
    // The pool was empty, someone might have been waiting for a connection
    pthread_cond_signal(&pool->connection_available);
  }

  pthread_mutex_unlock(&pool->cond_mutex);
}

