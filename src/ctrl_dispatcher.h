#ifndef LUX_CTRL_DISPATCHER_H
#define LUX_CTRL_DISPATCHER_H
#include "php_lux.h"
#include "zend_types.h"

typedef struct {
    char        cls[256];
    char        mth[128];
    zval        view; // 共享view object
    zend_object std;
} dispatcher_object_t;

#define Z_DISPATCHER_P(zv) ((dispatcher_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(dispatcher_object_t, std)))

void make_dispatcher(zval *hander_zv, const char *cls, const char *mth, zval *view);

LUX_MINIT_FUNCTION(Dispatcher);
#endif