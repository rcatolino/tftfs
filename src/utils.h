#ifndef UTILS_H__
#define UTILS_H__

#include <curl/curl.h>
#include <fuse.h>
#include <glib.h>
#include <stdio.h>

#include "http.h"

#define tft_handle ((struct tft_data*) fuse_get_context()->private_data)

#ifdef DEBUG
  #define debug(...) printf(__VA_ARGS__)
  #define log_debug(file, ...) if (file) fprintf(file, __VA_ARGS__)
#else
  #define debug(...)
  #define log_debug(file, ...)
#endif

struct tft_data {
  FILE * log;
  struct connection_pool *hpool;
};

enum opttype {
  STRING,
  LONG,
  NONE,
};

struct curlopt {
  long name;
  enum opttype type;
};

union curlparam {
  const char * strparam;
  long longparam;
  curl_off_t offset;
};

int isdir(const char * path);

void print_help_head();

// Logging :
FILE *log_init();
#define log(file, ...) if (file) fprintf(file, __VA_ARGS__)
#define fuse_debug(...) log_debug(tft_handle->log, __VA_ARGS__)
#define fuse_log(...) log(tft_handle->log, __VA_ARGS__)

// Option parsing :
int end_option(const char * opt);
union curlparam get_option_param(const char * param_name, enum opttype type, GHashTable *params);
int invalid_option(const char * opt, GHashTable *conditons);
int option(const char * opt);
#endif
