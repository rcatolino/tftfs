#define _GNU_SOURCE 1
#include <assert.h>
#include <curl/curl.h>
#include <fcntl.h>
#include <glib.h>
#include <openssl/sha.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "connections.h"
#include "http.h"
#include "utils.h"
#include "websocket.h"

#define MAGIC_STR "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
#define KEY_SZ 24
#define MAGIC_SZ 36
char *make_key() {
  /*
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
  */
  char *buff = malloc(KEY_SZ + 1);
  memcpy(buff, "dGhlIHNhbXBsZSBub25jZQ==", KEY_SZ+1);
  return buff;
}

void delete_key(char *key) {
  //g_free(key);
  free(key);
}

int check_key(const char *key, const char *accept) {
  // Both arg must be null terminated
  int ret = 0;
  unsigned char buffer[KEY_SZ+MAGIC_SZ];
  unsigned char result[SHA_DIGEST_LENGTH];
  char *result64;
  memcpy(buffer, key, KEY_SZ);
  memcpy(buffer+KEY_SZ, MAGIC_STR, MAGIC_SZ);
  SHA1(buffer, KEY_SZ+MAGIC_SZ, result);
  result64 = g_base64_encode((unsigned char *)result, SHA_DIGEST_LENGTH);
  ret = (strcmp(result64, accept) == 0) ? 1 : 0;
  fuse_debug("server accept : %s, computed key : %s\n", accept, result64);
  g_free(result64);
  return ret;
}

char *strip(char *str) {
  // str *must* be null-terminated
  char *ws = " ";
  char *stripped = str;
  // Move the start of string passed the spaces.
  stripped += strspn(str, ws);
  // Move the end of string before the spaces.
  stripped = strtok(stripped, ws);
  return stripped;
}

int parse_keyval(char *buffer, char **key, char **value) {
  // Buffer *must* be null terminated (key and value also btw).
  char *bkey = buffer;
  char *bval;
  // Extract value from the buffer as the string after ":"
  bkey = strtok_r(buffer, ":", &bval);
  if (!bkey) {
    return -1;
  }
  *key = strip(bkey);
  *value = strip(bval);
  return 0;
}

size_t head_callback(char *buffer, const char *ws_key) {
  // Buffer must be null terminated
  char *key, *val;

  if (parse_keyval(buffer, &key, &val) == -1) {
    return 0;
  }

  if ((strcasecmp(key, "Connection") == 0 && strcasecmp(val, "Upgrade") == 0) ||
      (strcasecmp(key, "Upgrade") == 0 && strcasecmp(val, "websocket") == 0)) {
    fuse_debug("upgrade header : %s, value : %s\n", key, val);
    return 1;
  } else if (strcasecmp(key, "Sec-WebSocket-Accept") == 0 && strcasecmp(val, "testaccept") == 0) {
    check_key(ws_key, val);
    return 1;
  }

  return 0;
}

uint64_t make_frame(char *buf, size_t buf_size, char flags, char opcode,
                 char *pl, uint64_t pl_len) {
  // Initialize the buffer as websocket frame and returns a pointer to the fram buffer.
  uint64_t frame_len = 6 + pl_len;
  if (buf_size < 6) {
    return 0; // Valid client-server frame minimum size
  }

  memset(buf, 0, buf_size);
  buf[0] |= (flags << 4)| opcode; // TODO: remove canary value
  buf[1] |= 0x01 << 7; // Mask, grrrr
  if (pl_len < 126) {
    if (buf_size < (pl_len + 2)) {
      return 0;
    }
    buf[1] |= pl_len;
    buf += 2;
  } else if (pl_len < (2^16)) {
    if (buf_size < (pl_len + 4)) {
      return 0;
    }
    buf[1] |= 126;
    pl_len = htobe16((uint16_t) pl_len);
    memcpy(buf+2, &pl_len, sizeof(uint16_t));
    buf += 4;
    frame_len += 2;
  } else {
    if (buf_size < pl_len + 10) {
      return 0;
    }
    buf[1] |= 127;
    pl_len = htobe64(pl_len);
    memcpy(buf+2, &pl_len, sizeof(uint64_t));
    buf += 10;
    frame_len += 4;
  }
  // We are going to use 0x2A,0x2A,0x2A,0x2A as a constant mask since this is
  // useless for trusted native clients anyway. This way we can simply use memfrob as
  // the xoring routine.
  memset(buf, 0x2A, 4);
  buf+=4;
  memcpy(buf, pl, pl_len);
  memfrob(buf, pl_len);
  return frame_len;

}

int wait_for_data(struct http_connection *con) {
  long socket;
  struct pollfd fds;
  CURLcode ret = curl_easy_getinfo(con->curl, CURLINFO_LASTSOCKET, &socket);
  if (ret == -1 || socket == -1) {
    fuse_debug("curl_easy_getinfo : invalid socket descriptor\n");
    return -1;
  }

  fds.fd = (int) socket;
  fds.events = POLLIN | POLLPRI;
  switch(poll(&fds, 1, 2000)) {
    case -1:
      fuse_debug("error waiting for data on websocket\n");
      return -1;
    case 0:
      fuse_debug("timeout waiting for data on websocket\n");
      return 0;
    default:
      if (fds.revents & (POLLIN | POLLPRI)) {
        fuse_debug("New data on websocket\n");
        return 1;
      }
      break;
  }

  return 0;
}
#define LITERAL_MSG(a) a, sizeof(a)

void test_connection(struct http_connection *con) {
  char frame_buf[125] = {0};
  size_t bytes_sent = 0;
  // Test the connection :
  ssize_t frame_len = make_frame(frame_buf, 125, WS_NONE, WS_TEXT, LITERAL_MSG("coucou!!"));
  assert(frame_len);
  CURLcode ret = curl_easy_send(con->curl,
                                frame_buf, frame_len,
                                &bytes_sent);
  fuse_debug("Websocket connection test send : %d. Sent %ld bytes.\n", ret, bytes_sent);
  bytes_sent = 0;
  if (wait_for_data(con) != 1) {
    return;
  }

  ret = curl_easy_recv(con->curl, frame_buf, 125, &bytes_sent);
  fuse_debug("Websocket connection test receive : %d. Received %ld bytes.\n", ret, bytes_sent);
  if (ret == CURLE_OK && bytes_sent != 0) {
    fuse_debug("Message : %.*s\n", (int)bytes_sent, frame_buf);
  } else fuse_debug("WebSocket connection reset by server\n");
}

int send_upgrade_request(struct http_connection *con) {
  size_t bytes_sent = 0;
  int host_len = strlen(con->host);
  char req[200+host_len];
  CURLcode ret;
  char *req_template = "GET /$websocket HTTP/1.1\r\n"
                       "Host: %s\r\n"
                       "Accept: */*\r\n"
                       "Upgrade: websocket\r\n"
                       "Connection: Upgrade\r\n"
                       "Sec-WebSocket-Key: %s\r\n"
                       "Sec-WebSocket-Version: 13\r\n\r\n";
  snprintf(req, 200+host_len, req_template, con->host, con->ws_key);
  fuse_debug("size of request : %ld\n", strlen(req));
  ret = curl_easy_send(con->curl, req, strlen(req), &bytes_sent);
  if (ret != 0) {
    fuse_debug("curl_easy_send error : %d\n", ret);
    return -1;
  }

  return 0;
}


int get_server_answer(struct http_connection *con) {
  char buffer[400] = {0};
  char *hline = buffer; // Pointer to a header line
  char *svptr; // Pointer for strtok
  size_t received = 0;
  long ret;
  if (wait_for_data(con) != 1) {
    return -1;
  }

  ret = curl_easy_recv(con->curl, buffer, 400, &received);
  if (ret != CURLE_OK) {
    fuse_debug("curl_easy_recv error : %ld\n", ret);
    return -1;
  } else if (received == 0) {
    fuse_debug("curl_easy_recv closed connection\n");
    return 0;
  }

  //fuse_debug("Answer received from the server : %.*s\n", (int)received, buffer);
  if (buffer[received-1] != '\n') {
    fuse_debug("get_server_answer : server answer incomplete\n");
    return -1;
  }
  buffer[received-1] = '\0';

  ret = 0;
  hline = strtok_r(buffer, "\r\n", &svptr);
  while (hline != NULL) {
    if (head_callback(hline, con->ws_key) == 1) {
      ret++;
    }

    hline = strtok_r(NULL, "\r\n", &svptr);
  }

  return ret;
}

struct http_connection *ws_upgrade(struct connection_pool *pool) {
  struct http_connection *con = acquire_connection(pool);

  curl_easy_setopt(con->curl, CURLOPT_HTTPGET, 1);
  curl_easy_setopt(con->curl, CURLOPT_CONNECT_ONLY, 1);
  make_action_url(con, "websocket");
  curl_easy_setopt(con->curl, CURLOPT_URL, con->last_result.effective_url);

  con->last_result.result = curl_easy_perform(con->curl);
  con->ws_key = make_key();
  if (con->last_result.result != 0 && con->last_result.result != 52 /*empty body*/) {
    // Test the network layer result
    fuse_debug("Curl error : %d\n", con->last_result.result);
    goto err;
  }

  // Perform the upgrade GET request
  if (send_upgrade_request(con) == -1) {
    fuse_debug("Network error sending upgrade request.\n");
    goto err;
  }

  if (get_server_answer(con) != 3) {
    fuse_debug("Bad server response.\n");
    goto err;
  }

  delete_key(con->ws_key);
  fuse_debug("Websocket upgrade success.\n");
  test_connection(con);
  return con;

err:
  delete_key(con->ws_key);
  release_connection(con, pool);
  return NULL;
}

void ws_close(struct http_connection *con, struct connection_pool *pool) {
  if (con) release_connection(con, pool);
}
