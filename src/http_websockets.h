#ifndef LUX_HTTP_WEBSOCKET_H
#define LUX_HTTP_WEBSOCKET_H
#include "php_lux.h"
#include "util/php_cb.h"

#include "http.h"
#include "zend_types.h"

#define WS_MESSAGE "message"
#define WS_IS_TEXT "isText"
#define WS_DATA "data"

typedef struct _ws_route_entry {
    char                   *path;
    php_cb_t               *on_open;
    php_cb_t               *on_message;
    php_cb_t               *on_close;
    struct _ws_route_entry *next;
} ws_route_entry_t;

void ws_on_open(ws_s *ws);
void ws_on_message(ws_s *ws, fio_str_info_s msg, uint8_t is_text);
void ws_on_close(intptr_t uuid, void *udata);

typedef struct {
    ws_s       *ws;
    zend_object std;
} http_websockets_object_t;

#define Z_HTTP_WS_OBJ_P(zv) ((http_websockets_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(http_websockets_object_t, std)));

LUX_MINIT_FUNCTION(HttpWebsockets);

#endif // LUX_HTTP_WEBSOCKET_H