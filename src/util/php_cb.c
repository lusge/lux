#include "php_cb.h"
#include "log.h"
#include "zend_API.h"
#include "zend_alloc.h"
#include "zend_exceptions.h"

#include "ext/standard/php_var.h"
#include "php_variables.h"
#include "util/php_cb.h"
#include "zend_types.h"
#include "zend_variables.h"
#include <_stdio.h>
#include <stdlib.h>
#include <string.h>

php_cb_t *php_cb_new(zval *z) {
    if (!z || Z_TYPE_P(z) == IS_UNDEF || Z_TYPE_P(z) == IS_NULL) {
        return NULL;
    }

    php_cb_t *p = malloc(sizeof(php_cb_t));

    if (!p) {
        return NULL;
    }
    memset(p, 0, sizeof(*p));
    ZVAL_COPY(&p->callable, z);

    if (zend_fcall_info_init(&p->callable, 0, &p->fci, &p->fcc, NULL, NULL) != SUCCESS) {
        zval_ptr_dtor(&p->callable);
        free(p);
        return NULL;
    }
    return p;
}

void php_cb_free(php_cb_t *p) {
    if (!p) {
        return;
    }
    zval_ptr_dtor(&p->callable);
    free(p);
}

int php_cb_call(php_cb_t *cb, int params_count, zval *params, zval *retval) {
    if (!cb) {
        return FAILURE;
    }

    ZVAL_UNDEF(retval);

    cb->fci.retval      = retval;
    cb->fci.param_count = params_count;
    cb->fci.params      = params;

    int result = zend_call_function(&cb->fci, &cb->fcc);
    if (EG(exception)) {
        zend_object *ex  = EG(exception);
        zval        *msg = zend_read_property(ex->ce, ex, "message", 7, 1, NULL);
        LOG_ERROR("PHP exception [%s]:%s", ZSTR_VAL(ex->ce->name), msg && Z_TYPE_P(msg) == IS_STRING ? Z_STRVAL_P(msg) : "?");
        zend_clear_exception();
    }

    return result;
}