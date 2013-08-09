#include <curl/curl.h>
#include <fcntl.h>
#include <glib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "connections.h"
#include "http.h"
#include "utils.h"
#include "websocket.h"

char *make_key() {
#define key_size 16 //Bytes
  char raw[key_size];
  int rd = open("/dev/urandom", O_RDONLY);
  if (rd == -1) {
    fuse_log("Error, cannot access /dev/urandom\n");
    return NULL;
  }

  if (read(rd, raw, key_size) == -1) {
    fuse_log("Error, cannot read from /dev/urandom\n");
    close(rd);
    return NULL;
  }

  return g_base64_encode((unsigned char *)raw, key_size);
}

void delete_key(char *key) {
  g_free(key);
}

char *strip(char *str) {
  char *ws = " ";
  char *stripped = str;
  stripped += strspn(str, ws);
  stripped = strtok(stripped, ws);
  return stripped;
}

#define H_Connection "Connection:"
#define H_Upgrade "Upgrade:"
#define H_SWA "Sec-WebSocket-Accept:"
#define length(a) sizeof(a)-1
size_t head_callback(char *buffer, size_t block_size, size_t nbblock, void *userdata) {
  size_t buf_size = block_size*nbblock;
  int *ret = (int *)userdata;

  if (strncmp(buffer, H_Connection, length(H_Connection)) == 0) {
    char *value = strip(buffer+length(H_Connection));
    if (strcasecmp(value, "Upgrade")) (*ret)++;
  } else if (strncmp(buffer, H_Upgrade, length(H_Upgrade)) == 0) {
    char *value = strip(buffer+length(H_Upgrade));
    if (strcasecmp(value, "websocket")) (*ret)++;
  } else if (strncmp(buffer, H_SWA, length(H_SWA)) == 0) {
    char *value = strip(buffer+length(H_SWA));
    fuse_debug("Sec-WebSocket-Accept : %s\n", value);
    (*ret)++;
  }

  return buf_size;
}

struct http_connection *ws_upgrade(struct connection_pool *pool) {
  struct curl_slist *headers = NULL;
  long ret;
  int callback_ret = 0;
  char *ws_key;
  char key_header[50] = {0};
  struct http_connection *con = acquire_connection(pool);

  curl_easy_setopt(con->curl, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(con->curl, CURLOPT_HEADERDATA, &callback_ret);
  curl_easy_setopt(con->curl, CURLOPT_HEADERFUNCTION, head_callback);
  curl_easy_setopt(con->curl, CURLOPT_WRITEFUNCTION, NULL);
  make_action_url(con, "websocket");
  curl_easy_setopt(con->curl, CURLOPT_URL, con->last_result.effective_url);

  headers = curl_slist_append(headers, "Connection:Upgrade");
  headers = curl_slist_append(headers, "Upgrade:websocket");
  headers = curl_slist_append(headers, "Sec-WebSocket-Version:13");
  ws_key = make_key();
  strcat(strcat(key_header, "Sec-WebSocket-Key:"), ws_key);
  headers = curl_slist_append(headers, key_header);
  curl_easy_setopt(con->curl, CURLOPT_HTTPHEADER, headers);

  con->last_result.result = curl_easy_perform(con->curl);
  ret = http_get_resp_code(con);
  if (con->last_result.result != 0 && con->last_result.result != 52 /*empty body*/) {
    // Test the network layer result
    fuse_debug("Curl error : %d\n", con->last_result.result);
  } else if (ret != 101) {
    // Test the application layer result
    fuse_debug("The server didn't upgrade but returned %ld\n", ret);
  } else if (callback_ret != 3) {
    fuse_debug("The server response is invalid\n");
  }

  delete_key(ws_key);
  curl_easy_setopt(con->curl, CURLOPT_HEADERFUNCTION, NULL);
  curl_easy_setopt(con->curl, CURLOPT_HTTPHEADER, NULL);
  curl_easy_setopt(con->curl, CURLOPT_HEADERDATA, NULL);
  curl_easy_setopt(con->curl, CURLOPT_HEADER, 0);
  curl_slist_free_all(headers);
  return con;
}

void ws_close(struct http_connection *con, struct connection_pool *pool) {
  release_connection(con, pool);
}
