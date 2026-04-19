#ifndef LUX_VIEW_H
#define LUX_VIEW_H
#include "php_lux.h"

typedef struct {
    char       *view_path;
    char       *layout;
    zend_object std;
} view_object_t;

#define Z_VIEW_P(zv) \
    ((view_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(view_object_t, std)))

extern zend_class_entry *lux_view_ce;

LUX_MINIT_FUNCTION(View);
#endif