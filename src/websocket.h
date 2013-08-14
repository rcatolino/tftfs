#ifndef WEBSOCKET_H__
#define WEBSOCKET_H__

#include "connections.h"
#include "http.h"

// Websocket framing protocol. cf https://tools.ietf.org/html/rfc6455#section-5.2
// Flags :
#define WS_NONE 0x0
#define WS_FIN 0x1
#define WS_RSV1 0x2
#define WS_RSV2 0x4
#define WS_RSV3 0x8
// Opcode :
#define WS_CONT 0x0
#define WS_TEXT 0x1
#define WS_BINARY 0x2
#define WS_CLOSE 0x8
#define WS_PING 0x9
#define WS_PONG 0xA


struct http_connection *ws_upgrade(struct connection_pool *con);
void ws_close(struct http_connection *con, struct connection_pool *pool);
int ws_send(struct http_connection *con);
int ws_recv(struct http_connection *con);

#endif
