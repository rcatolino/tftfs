#include <assert.h>
#include <curl/curl.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "http.h"
#include "utils.h"

#define add_param(params, key, value) hvalue = malloc(sizeof(long));\
                                      *hvalue = value;\
                                      hkey = malloc(sizeof(key));\
                                      strcpy(hkey, key);\
                                      g_hash_table_insert(params, hkey, hvalue)

#define add_option(options, key, value, otype) hvalue = malloc(sizeof(struct curlopt));\
                                              hvalue->name = value;\
                                              hvalue->type = otype;\
                                              hkey = malloc(sizeof(key));\
                                              strcpy(hkey, key);\
                                              g_hash_table_insert(options, hkey, hvalue)

GHashTable * init_http_parameters() {
  GHashTable * params = g_hash_table_new_full(g_str_hash,
                                               g_str_equal,
                                               (GDestroyNotify)free,
                                               (GDestroyNotify)free);
  long * hvalue;
  char * hkey;
  add_param(params, "basic", CURLAUTH_BASIC);
  add_param(params, "digest", CURLAUTH_DIGEST);
  add_param(params, "auto", CURLAUTH_ANY);

  return params;
}

GHashTable * init_http_options() {
  GHashTable * options = g_hash_table_new_full(g_str_hash,
                                               g_str_equal,
                                               (GDestroyNotify)free,
                                               (GDestroyNotify)free);
  struct curlopt * hvalue;
  char * hkey;
  add_option(options, "proxy", CURLOPT_PROXY, STRING);
  add_option(options, "proxy-auth", CURLOPT_PROXYAUTH, LONG);
  add_option(options, "proxy-username", CURLOPT_PROXYUSERNAME, STRING);
  add_option(options, "proxy-password", CURLOPT_PROXYPASSWORD, STRING);
  add_option(options, "http-auth", CURLOPT_HTTPAUTH, LONG);
  add_option(options, "username", CURLOPT_USERNAME, STRING);
  add_option(options, "password", CURLOPT_PASSWORD, STRING);

  return options;
}

struct http_data *http_init(int *argc, char *(*argv[])) {
  struct http_data *data = NULL;
  CURL * curl;
  int targc = *argc;
  char **targv = *argv;
  CURLcode req_res;
  GHashTable * options;
  GHashTable * parameters;

  int ret = curl_global_init(CURL_GLOBAL_ALL);
  if (ret != 0) {
    return NULL;
  }

  curl = curl_easy_init();
  if (!curl) {
    curl_global_cleanup();
    return NULL;
  }

  options = init_http_options();
  parameters = init_http_parameters();
  for (int i = 1; i < targc; i++) {
    if (end_option(targv[i])) {
      *argc -= 1;
      *argv += 1;
      break;
    }

    if (option(targv[i])) {
      struct curlopt *co;
      union curlparam param;
      if (invalid_option(targv[i], options)) {
        printf("Invalid option %s.\n", targv[i]);
        goto err;
      }

      co = g_hash_table_lookup(options, targv[i]+2);
      assert(co);
      param = get_option_param(targv[i+1], co->type, parameters);
      if (param.longparam == -1) {
        printf("Invalid argument %s for option %s\n", targv[i+1], targv[i]);
        goto err;
      }

      curl_easy_setopt(curl, co->name, param);
      // We just consumed two args :
      *argc -= 2;
      *argv += 2;
    } else {
      // End of options
      break;
    }
  }

  *argv[0] = targv[0]; // Replace the name of the programm as the first element.

  if (*argc < 3) {
    printf("Error, you need to specify a tree url and a mount point on the local filesystem.\n");
    print_help_head();
    goto err;
  }

  curl_easy_setopt(curl, CURLOPT_URL, (*argv)[1]);
  // Test that the url is valid and that we can connect
  req_res = curl_easy_perform(curl);
  if (req_res != CURLE_OK) {
    printf("could not connect to %s : %s\n", (*argv)[1], curl_easy_strerror(req_res));
    goto err;
  }

  data = malloc(sizeof(struct http_data));
  data->curl = curl;
  g_hash_table_unref(parameters);
  g_hash_table_unref(options);
  return data;

err:
  g_hash_table_unref(parameters);
  g_hash_table_unref(options);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return NULL;
}

void http_cleanup(struct http_data *data) {
  if (!data) {
    return;
  }

  curl_easy_cleanup(data->curl);
  curl_global_cleanup();
  free(data);
}
