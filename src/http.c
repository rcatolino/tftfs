#define _GNU_SOURCE 1
#include <assert.h>
#include <glib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "connections.h"
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

static size_t writefunction(char *buf, size_t blck_size, size_t nmemb, void *userdata) {
  fuse_debug("%.*s", (int)(blck_size*nmemb), buf);
  return blck_size*nmemb;
}

static size_t print_header_callback(char *buf, size_t blck_size, size_t nmemb, void *unused) {
  debug("%.*s", (int)(blck_size*nmemb), buf);
  return blck_size*nmemb;
}


static GHashTable * init_http_parameters() {
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

static GHashTable * init_http_options() {
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

static char * normalize(char *url) {
  // For now this just removes any trailing '/'
  // I guess this should probably collapse any '../'
  // as well, but it might not really be useful.
  if (strlen(url) == 0) {
    return url;
  }

  if (url[strlen(url)-1] == '/') {
    url[strlen(url)-1] = '\0';
  }

  return url;
}

struct connection_pool *http_init(int *argc, char *(*argv[])) {
  struct http_connection *data = NULL;
  CURL * curl;
  int targc = *argc;
  char **targv = *argv;
  CURLcode req_res;
  GHashTable * options;
  GHashTable * parameters;
  long curl_response;
  int scheme_head = 0;

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
      i++; // do not treat the next arg, it was a option argument
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

  // Verify that the url is http/https :
#define strn(a) a, sizeof(a)-1
  if (strncmp((*argv)[1], strn("http://")) == 0) {
    scheme_head = sizeof("http://") - 1;
  } else if (strncmp((*argv)[1], strn("https://")) == 0) {
    scheme_head = sizeof("https://") - 1;
  } else if (strstr((*argv)[1], "://") == NULL) {
    scheme_head = 0;
  } else {
    char delim = ':';
    printf("Error, unsuported protocol : %s.\n", strtok((*argv)[1], &delim));
    goto err;
  }

  debug("Connecting to %s\n", (*argv)[1]);
  curl_easy_setopt(curl, CURLOPT_URL, (*argv)[1]);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1);
  curl_easy_setopt(curl, CURLOPT_NOBODY, 1);
  curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, print_header_callback);
  // Test that the url is valid and that we can connect
  req_res = curl_easy_perform(curl);
  if (req_res != CURLE_OK) {
    printf("Could not connect to %s : %s\n", (*argv)[1], curl_easy_strerror(req_res));
    goto err;
  }

  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &curl_response);
  if (curl_response != 200) {
    printf("HTTP Error %ld\n", curl_response);
    goto err;
  }

  data = malloc(sizeof(struct http_connection));
  data->curl = curl;
  data->root_url = malloc(strlen((*argv)[1]));
  strcpy(data->root_url, normalize((*argv)[1]));
  data->root_path = strchrnul(data->root_url+scheme_head, '/');
#define host_size() data->root_path - data->root_url + 1
  data->host = malloc(host_size());
  memcpy(data->host, data->root_url, host_size());
  data->host[host_size() - 1] = '\0';
  data->last_result.callback_userdata = NULL;
  data->last_result.effective_url = NULL;
  curl_easy_setopt(data->curl, CURLOPT_WRITEDATA, data->last_result.callback_userdata);
  g_hash_table_unref(parameters);
  g_hash_table_unref(options);

  return create_pool(data);

err:
  g_hash_table_unref(parameters);
  g_hash_table_unref(options);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  return NULL;
}

void http_cleanup(struct http_connection *data) {
  if (!data) {
    return;
  }

  curl_easy_cleanup(data->curl);
  if (data->last_result.effective_url) free(data->last_result.effective_url);
  free(data);
}

long http_get_resp_code(struct http_connection* con) {
  long curl_response;
  curl_easy_getinfo(con->curl, CURLINFO_RESPONSE_CODE, &curl_response);
  return curl_response;
}

void make_action_url(struct http_connection *con, const char *action) {
  int host_length = strlen(con->host);
  int url_size = strlen(action) + host_length + 3;

  assert(action);
  if (con->last_result.effective_url) {
    con->last_result.effective_url = realloc(con->last_result.effective_url, url_size);
  } else {
    con->last_result.effective_url = malloc(url_size);
  }

  memset(con->last_result.effective_url, 0, url_size);
  strcat(con->last_result.effective_url, con->host);
  strcat(con->last_result.effective_url, "/$");
  strcat(con->last_result.effective_url, action);
}

struct http_connection *post(struct connection_pool *pool, const char *action, const char *data,
                             result_callback callback, void *userdata) {
  struct http_connection *con = acquire_connection(pool);
  // Construct post url :
  make_action_url(con, action);
  //fuse_debug("sending %s to %s.\n", data, con->last_result.effective_url);

  // Set curl options
  curl_easy_setopt(con->curl, CURLOPT_URL, con->last_result.effective_url);
  curl_easy_setopt(con->curl, CURLOPT_WRITEFUNCTION, callback ? callback : writefunction);
  curl_easy_setopt(con->curl, CURLOPT_WRITEDATA, userdata ? userdata : con->last_result.callback_userdata);
  curl_easy_setopt(con->curl, CURLOPT_POSTFIELDS, data);
  curl_easy_setopt(con->curl, CURLOPT_POST, 1);
  con->last_result.result = curl_easy_perform(con->curl);
  return con;
}

void release(struct connection_pool* pool, struct http_connection *con) {
  release_connection(con, pool);
}
