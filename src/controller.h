#ifndef LUX_CONTROLLER_H
#define LUX_CONTROLLER_H
#include "php_lux.h"

typedef struct {
    zval        view;
    zval        request;
    zval        response;
    zend_object std;
} controller_object_t;

#define Z_CONTROLLER_P(zv) \
    ((controller_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(controller_object_t, std)))

extern zend_class_entry *lux_controller_ce;

void controller_inject(zval *ctrl_zv, zval *req, zval *resp, zval *view);

LUX_MINIT_FUNCTION(Controller);
#endif