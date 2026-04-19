#ifndef LUX_APP_H
#define LUX_APP_H
#include "php_lux.h"

typedef struct {
    char       *host;
    zend_long   port;
    int         num_workers;
    zval        view;
    zval        router;
    char        view_path[4096];
    zend_object std;
} app_object_t;

#define Z_APP_OBJ_P(zv) Z_APP_P(Z_OBJ_P(zv))

#define Z_APP_P(obj) (app_object_t *)((char *)(obj) - XtOffsetOf(app_object_t, std))

LUX_MINIT_FUNCTION(App);

#endif