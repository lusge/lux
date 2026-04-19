#ifndef LUX_HTTP_REQUEST_H
#define LUX_HTTP_REQUEST_H
#include "http.h"
#include "php_lux.h"

typedef struct {
    http_s     *req;
    zend_object std;
} http_request_object_t;

extern zend_class_entry *lux_http_request_ce;

void build_request(zval *out, http_s *req, zval *params);

#define Z_HTTP_REQUEST_P(zv) \
    ((http_request_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(http_request_object_t, std)))

LUX_MINIT_FUNCTION(HttpRequest);

#endif // LUX_HTTP_REQUEST_H