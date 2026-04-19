#include "ctrl_dispatcher.h"
#include "controller.h"
#include "standard/php_var.h"
#include "util/log.h"
#include "zend_API.h"
#include "zend_alloc.h"
#include "zend_exceptions.h"
#include "zend_execute.h"
#include "zend_interfaces.h"
#include "zend_object_handlers.h"
#include "zend_objects.h"
#include "zend_objects_API.h"
#include "zend_portability.h"
#include "zend_types.h"
#include "zend_variables.h"
#include <string.h>

zend_class_entry           *lux_ctrl_dispatcher_ce;
static zend_object_handlers lux_ctrl_dispatcher_handlers;

static zend_object *ctrl_dispatcher_class_new(zend_class_entry *ce) {
    dispatcher_object_t *obj = ecalloc(1, sizeof(dispatcher_object_t) + zend_object_properties_size(ce));
    zend_object_std_init(&obj->std, ce);
    obj->std.handlers = &lux_ctrl_dispatcher_handlers;
    ZVAL_UNDEF(&obj->view);

    return &obj->std;
}

static void ctrl_dispatcher_class_free(zend_object *o) {
    dispatcher_object_t *obj = (dispatcher_object_t *)((char *)o - XtOffsetOf(dispatcher_object_t, std));
    zval_ptr_dtor(&obj->view);
    zend_object_std_dtor(o);
}

static void dispatcher_controller(const char *cls, const char *mth, zval *req, zval *resp, zval *view) {
    zend_string      *cls_name = zend_string_init(cls, strlen(cls), 0);
    zend_class_entry *ce       = zend_lookup_class(cls_name);
    zend_string_release(cls_name);

    if (!ce) {
        zend_throw_exception_ex(NULL, 0, "Controller not found %s !", cls);
        return;
    }

    zval ctrl_zv;
    object_init_ex(&ctrl_zv, ce);

    // php_var_dump(&ctrl_zv, 1);
    if (ce->constructor) {
        zval ignored;
        zend_call_method_with_0_params(Z_OBJ(ctrl_zv), ce, NULL, "__construct", &ignored);
        zval_ptr_dtor(&ignored);
    }

    // 设置controller 内容
    controller_inject(&ctrl_zv, req, resp, view);

    zval retval;
    ZVAL_UNDEF(&retval);
    zend_call_method_with_1_params(Z_OBJ(ctrl_zv), ce, NULL, mth, &retval, req);
    return;
    zval_ptr_dtor(&retval);
    zval_ptr_dtor(&ctrl_zv);
}

void make_dispatcher(zval *hander_zv, const char *cls, const char *mth, zval *view) {

    object_init_ex(hander_zv, lux_ctrl_dispatcher_ce);
    dispatcher_object_t *obj = Z_DISPATCHER_P(hander_zv);
    strncpy(obj->cls, cls, sizeof(obj->cls) - 1);
    strncpy(obj->mth, mth, sizeof(obj->mth) - 1);

    if (view && Z_TYPE_P(view) != IS_UNDEF) {
        ZVAL_COPY(&obj->view, view);
    }
}

PHP_METHOD(CtrlDispatcher, __invoke) {
    zval *req, *resp;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_OBJECT(req)
    Z_PARAM_OBJECT(resp)
    ZEND_PARSE_PARAMETERS_END();

    dispatcher_object_t *obj = Z_DISPATCHER_P(getThis());

    dispatcher_controller(obj->cls, obj->mth, req, resp, Z_TYPE(obj->view) != IS_UNDEF ? &obj->view : NULL);
}

ZEND_BEGIN_ARG_INFO_EX(ai_dispatcher_invoke, 0, 0, 2)
ZEND_ARG_INFO(0, req)
ZEND_ARG_INFO(0, resp)
ZEND_END_ARG_INFO()

static const zend_function_entry dispatcher_methods[] = {
    PHP_ME(CtrlDispatcher, __invoke, ai_dispatcher_invoke, ZEND_ACC_PUBLIC)
        PHP_FE_END};

LUX_MINIT_FUNCTION(Dispatcher) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\ControllerDispatcher", dispatcher_methods);
    lux_ctrl_dispatcher_ce                = zend_register_internal_class(&ce);
    lux_ctrl_dispatcher_ce->create_object = ctrl_dispatcher_class_new;

    memcpy(&lux_ctrl_dispatcher_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_ctrl_dispatcher_handlers.offset   = XtOffsetOf(dispatcher_object_t, std);
    lux_ctrl_dispatcher_handlers.free_obj = ctrl_dispatcher_class_free;
    return SUCCESS;
}