#ifndef LUX_PHP_CB_H
#define LUX_PHP_CB_H
#include "php.h"

typedef struct {
    zend_fcall_info       fci;
    zend_fcall_info_cache fcc;
    zval                  callable;
} php_cb_t;

php_cb_t *php_cb_new(zval *z);
void      php_cb_free(php_cb_t *p);
int       php_cb_call(php_cb_t *cb, int params_count, zval *params, zval *retval);

#endif // CEVRO_PHP_CB_H