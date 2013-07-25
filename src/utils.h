#ifndef UTILS_H__
#define UTILS_H__

#include <curl/curl.h>
#include <glib.h>

#ifdef DEBUG
  #define debug(...) printf(__VA_ARGS__)
#else
  #define debug(...)
#endif

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

// Option parsing :
int end_option(const char * opt);
union curlparam get_option_param(const char * param_name, enum opttype type, GHashTable *params);
int invalid_option(const char * opt, GHashTable *conditons);
int option(const char * opt);
#endif
