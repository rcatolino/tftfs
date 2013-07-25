#ifndef HTTP_H__
#define HTTP_H__

#include <curl/curl.h>

struct http_data {
  CURL * curl;
};

void http_cleanup(struct http_data *data);
struct http_data *http_init(int * argc, char *(*argv[]));

#endif
