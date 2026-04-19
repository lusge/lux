#include "http_websockets.h"
#include "http.h"
#include "php.h"
#include "util/log.h"
#include "util/php_cb.h"
#include "websockets.h"
#include "zend_API.h"
#include "zend_long.h"
#include "zend_types.h"
#include "zend_variables.h"
#include <stdio.h>

zend_class_entry           *lux_http_websockets_ce;
static zend_object_handlers lux_http_websockets_handlers;

static void build_websockets(zval *out, ws_s *ws);

void ws_on_open(ws_s *ws) {
    ws_route_entry_t *ws_route = (ws_route_entry_t *)websocket_udata_get(ws);

    if (ws_route->on_open) {
        zval ws_zv;
        build_websockets(&ws_zv, ws);
        zval params[1];
        ZVAL_COPY_VALUE(&params[0], &ws_zv);
        zval retval;
        php_cb_call(ws_route->on_open, 1, params, &retval);
        zval_ptr_dtor(&ws_zv);
        zval_ptr_dtor(&retval);
    }
}

void ws_on_message(ws_s *ws, fio_str_info_s msg, uint8_t is_text) {
    ws_route_entry_t *ws_route = (ws_route_entry_t *)websocket_udata_get(ws);

    if (ws_route->on_message) {
        zval ws_zv;
        build_websockets(&ws_zv, ws);
        zend_update_property_string(lux_http_websockets_ce, Z_OBJ_P(&ws_zv), WS_MESSAGE, sizeof(WS_MESSAGE) - 1, estrndup(msg.data, msg.len));
        zend_update_property_long(lux_http_websockets_ce, Z_OBJ_P(&ws_zv), WS_IS_TEXT, sizeof(WS_IS_TEXT) - 1, (zend_long)is_text);
        zval params[1];
        ZVAL_COPY_VALUE(&params[0], &ws_zv);
        zval retval;
        php_cb_call(ws_route->on_message, 1, params, &retval);
        zval_ptr_dtor(&ws_zv);
        zval_ptr_dtor(&retval);
    }
}

void ws_on_close(intptr_t uuid, void *udata) {
    ws_route_entry_t *ws_route = (ws_route_entry_t *)udata;

    if (ws_route->on_close) {
        zval params[1];
        ZVAL_LONG(&params[0], (zend_long)uuid);
        zval retval;
        php_cb_call(ws_route->on_close, 1, params, &retval);
        zval_ptr_dtor(&retval);
    }
}

static zend_object *http_websocket_new(zend_class_entry *ce) {
    http_websockets_object_t *obj = ecalloc(1, sizeof(http_websockets_object_t) + zend_object_properties_size(ce));
    zend_object_std_init(&obj->std, ce);
    obj->std.handlers = &lux_http_websockets_handlers;
    return &obj->std;
}

static void build_websockets(zval *out, ws_s *ws) {
    object_init_ex(out, lux_http_websockets_ce);
    http_websockets_object_t *obj = Z_HTTP_WS_OBJ_P(out);
    obj->ws                       = ws;
}

PHP_METHOD(HttpWebsockets, subscribe) {
    char  *message;
    size_t ml;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(message, ml)
    ZEND_PARSE_PARAMETERS_END();

    http_websockets_object_t *obj = Z_HTTP_WS_OBJ_P(getThis());

    websocket_subscribe(obj->ws, .channel = (fio_str_info_s){.data = message, .len = ml});
}

PHP_METHOD(HttpWebsockets, write) {
    char  *message;
    size_t ml;

    zend_long isText = 1;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(message, ml)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(isText)
    ZEND_PARSE_PARAMETERS_END();

    http_websockets_object_t *obj = Z_HTTP_WS_OBJ_P(getThis());

    websocket_write(obj->ws, (fio_str_info_s){.data = message, .len = ml},
                    (uint8_t)isText);
}

ZEND_BEGIN_ARG_INFO_EX(hw_message, 0, 0, 1)
ZEND_ARG_INFO(0, message)
ZEND_END_ARG_INFO()

static const zend_function_entry http_ws_methods[] = {
    PHP_ME(HttpWebsockets, subscribe, hw_message, ZEND_ACC_PUBLIC)
        PHP_ME(HttpWebsockets, write, hw_message, ZEND_ACC_PUBLIC)
            PHP_FE_END};

LUX_MINIT_FUNCTION(HttpWebsockets) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\Http\\Websocket", http_ws_methods);

    lux_http_websockets_ce                = zend_register_internal_class(&ce);
    lux_http_websockets_ce->create_object = http_websocket_new;

    memcpy(&lux_http_websockets_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_http_websockets_handlers.offset = XtOffsetOf(http_websockets_object_t, std);

    zend_declare_property_long(lux_http_websockets_ce, WS_IS_TEXT, sizeof(WS_IS_TEXT) - 1, -1, ZEND_ACC_PUBLIC);
    zend_declare_property_string(lux_http_websockets_ce, WS_MESSAGE, sizeof(WS_MESSAGE) - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_string(lux_http_websockets_ce, WS_DATA, sizeof(WS_DATA) - 1, "", ZEND_ACC_PUBLIC);
    return SUCCESS;
}