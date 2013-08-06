#ifndef WEBSOCKET_H__
#define WEBSOCKET_H__

#include "connections.h"
#include "http.h"

struct http_connection *ws_upgrade(struct connection_pool *con);
void ws_close(struct http_connection *con, struct connection_pool *pool);
int ws_send(struct http_connection *con);
int ws_recv(struct http_connection *con);

#endif
