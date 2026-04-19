#ifndef LUX_HTTP_RESPONSE_H
#define LUX_HTTP_RESPONSE_H
#include "http.h"
#include "php_lux.h"
#include "zend_types.h"

#define RESP_STATUS_CODE "statusCode"
#define RESP_BODY "body"
#define RESP_HEASERS "headers"

#define Z_HTTP_RESPONSE_OBJ_P(zv) ((http_response_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(http_response_object_t, std)));

void build_response(zval *out);

void response_set_status_code(zval *resp_zv, int statusCode);

void response_set_body(zval *resp_zv, const char *body, size_t body_len);

void send_response(http_s *req, zval *resp_zv);

LUX_MINIT_FUNCTION(HttpResponse);

#endif