#include "controller.h"
#include "ext/json/php_json.h"
#include "http_response.h"
#include "standard/php_var.h"
#include "view.h"
#include "zend_API.h"
#include "zend_types.h"
#include "zend_variables.h"

zend_class_entry           *lux_controller_ce;
static zend_object_handlers lux_controller_handlers;

static void controller_free_obj(zend_object *o) {
    controller_object_t *c = (controller_object_t *)((char *)o - XtOffsetOf(controller_object_t, std));
    zval_ptr_dtor(&c->view);
    zval_ptr_dtor(&c->request);
    zval_ptr_dtor(&c->response);
    zend_object_std_dtor(o);
}

static zend_object *controller_new(zend_class_entry *ce) {
    controller_object_t *c = ecalloc(1, sizeof(controller_object_t) +
                                            zend_object_properties_size(ce));
    zend_object_std_init(&c->std, ce);
    object_properties_init(&c->std, ce);
    c->std.handlers = &lux_controller_handlers;
    ZVAL_UNDEF(&c->view);
    ZVAL_UNDEF(&c->request);
    ZVAL_UNDEF(&c->response);
    return &c->std;
}

void controller_inject(zval *ctrl_zv, zval *req, zval *resp, zval *view) {
    controller_object_t *c = Z_CONTROLLER_P(ctrl_zv);
    if (req) {
        ZVAL_COPY(&c->request, req);
    }

    if (resp) {
        ZVAL_COPY(&c->response, resp);
    }

    if (view && Z_TYPE_P(view) != IS_UNDEF) {
        ZVAL_COPY(&c->view, view);
    }
}

PHP_METHOD(Controller, request) {
    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->request) == IS_UNDEF)
        RETURN_NULL();
    ZVAL_COPY(return_value, &c->request);
}

PHP_METHOD(Controller, response) {
    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->response) == IS_UNDEF)
        RETURN_NULL();
    ZVAL_COPY(return_value, &c->response);
}

PHP_METHOD(Controller, render) {
    char  *tpl;
    size_t tpl_len;
    zval  *data = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(tpl, tpl_len)
    Z_PARAM_OPTIONAL Z_PARAM_ARRAY(data)
        ZEND_PARSE_PARAMETERS_END();

    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->view) == IS_UNDEF) {
        zend_throw_exception(NULL, "Controller: view engine not configured", 0);
        return;
    }

    zval tpl_zv, retval;
    ZVAL_STRINGL(&tpl_zv, tpl, tpl_len);

    if (data) {
        zend_call_method_with_2_params(Z_OBJ(c->view), lux_view_ce, NULL, "render", &retval, &tpl_zv, data);
    } else {
        zend_call_method_with_1_params(Z_OBJ(c->view), lux_view_ce, NULL, "render", &retval, &tpl_zv);
    }
    zval_ptr_dtor(&tpl_zv);

    if (Z_TYPE(retval) == IS_STRING) {
        // 设置状态码
        response_set_status_code(&c->response, 200);

        zend_call_method_with_1_params(Z_OBJ(c->response), Z_OBJCE(c->response), NULL, "html", return_value, &retval);
    }

    zval_ptr_dtor(&retval);
}

PHP_METHOD(Controller, json) {
    zval     *data;
    zend_long status = 200;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_ARRAY(data)
    Z_PARAM_OPTIONAL Z_PARAM_LONG(status)
        ZEND_PARSE_PARAMETERS_END();

    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->response) == IS_UNDEF)
        return;

    zval sv;
    ZVAL_LONG(&sv, status);
    zval ignored;
    zend_call_method_with_1_params(Z_OBJ(c->response), Z_OBJCE(c->response), NULL, "status", &ignored, &sv);
    zval_ptr_dtor(&ignored);
    zend_call_method_with_1_params(Z_OBJ(c->response), Z_OBJCE(c->response), NULL, "json", return_value, data);
}

PHP_METHOD(Controller, text) {
    char     *text;
    size_t    text_len;
    zend_long status = 200;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(text, text_len)
    Z_PARAM_OPTIONAL Z_PARAM_LONG(status)
        ZEND_PARSE_PARAMETERS_END();

    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->response) == IS_UNDEF)
        return;

    zval sv, tv, ignored;
    ZVAL_LONG(&sv, status);
    ZVAL_STRINGL(&tv, text, text_len);
    zend_call_method_with_1_params(Z_OBJ(c->response), Z_OBJCE(c->response), NULL, "status", &ignored, &sv);
    zval_ptr_dtor(&ignored);
    zend_call_method_with_1_params(Z_OBJ(c->response), Z_OBJCE(c->response), NULL, "text", return_value, &tv);
    zval_ptr_dtor(&tv);
}

PHP_METHOD(Controller, html) {
    char     *html;
    size_t    html_len;
    zend_long status = 200;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(html, html_len)
    Z_PARAM_OPTIONAL Z_PARAM_LONG(status)
        ZEND_PARSE_PARAMETERS_END();

    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->response) == IS_UNDEF) {
        return;
    }

    // 设置状态码
    response_set_status_code(&c->response, status);

    zval body;
    ZVAL_STRINGL(&body, html, html_len);
    zend_call_method_with_1_params(Z_OBJ(c->response),
                                   Z_OBJCE(c->response), NULL, "html", return_value, &body);
    zval_ptr_dtor(&body);
}

PHP_METHOD(Controller, redirect) {
    char     *url;
    size_t    url_len;
    zend_long status = 302;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(url, url_len)
    Z_PARAM_OPTIONAL Z_PARAM_LONG(status)
        ZEND_PARSE_PARAMETERS_END();

    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->response) == IS_UNDEF)
        return;

    zval sv, ignored;
    ZVAL_LONG(&sv, status);
    zend_call_method_with_1_params(Z_OBJ(c->response), Z_OBJCE(c->response), NULL, "status", &ignored, &sv);
    zval_ptr_dtor(&ignored);

    zval k, v, ignored2;
    ZVAL_STRING(&k, "Location");
    ZVAL_STRINGL(&v, url, url_len);
    zend_call_method_with_2_params(Z_OBJ(c->response),
                                   Z_OBJCE(c->response), NULL, "header", &ignored2, &k, &v);
    zval_ptr_dtor(&k);
    zval_ptr_dtor(&v);
    zval_ptr_dtor(&ignored2);
    RETURN_TRUE;
}

/* 读取请求输入（query + JSON body）*/
PHP_METHOD(Controller, input) {
    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->request) == IS_UNDEF) {
        RETURN_NULL();
    }

    zend_call_method_with_0_params(Z_OBJ(c->request), Z_OBJCE(c->request), NULL, "input", return_value);
}

PHP_METHOD(Controller, param) {
    char  *key;
    size_t key_len;
    zval  *def = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(key, key_len)
    Z_PARAM_OPTIONAL Z_PARAM_ZVAL(def)
        ZEND_PARSE_PARAMETERS_END();

    /* params 由 ControllerDispatcher 通过 request 属性 "routeParams" 传入 */
    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->request) == IS_UNDEF) {
        RETURN_NULL();
    }

    zval sv, ignored;
    ZVAL_STRINGL(&sv, key, key_len);
    zend_call_method_with_1_params(Z_OBJ(c->request), Z_OBJCE(c->request), NULL, "param", &ignored, &sv);

    if (Z_TYPE(ignored) != IS_UNDEF || Z_TYPE(ignored) != IS_NULL) {
        ZVAL_COPY(return_value, &ignored);
    }
    zval_ptr_dtor(&ignored);

    if (def)
        ZVAL_COPY(return_value, def);
    else
        RETURN_NULL();
}

PHP_METHOD(Controller, params) {
    controller_object_t *c = Z_CONTROLLER_P(getThis());

    zval ignored;
    zend_call_method_with_0_params(Z_OBJ(c->request), Z_OBJCE(c->request), NULL, "params", &ignored);
    if (Z_TYPE(ignored) != IS_UNDEF || Z_TYPE(ignored) != IS_NULL) {
        ZVAL_COPY(return_value, &ignored);
    }
    zval_ptr_dtor(&ignored);
}

/* 获取所有请求 header */
PHP_METHOD(Controller, headers) {
    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->request) == IS_UNDEF) {
        RETURN_NULL();
    }
    zval *hdrs = zend_read_property(Z_OBJCE(c->request), Z_OBJ(c->request),
                                    "headers", sizeof("headers") - 1, 0, NULL);
    ZVAL_COPY(return_value, hdrs);
}

PHP_METHOD(Controller, post) {
    char  *key;
    size_t key_len;
    zval  *def = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(key, key_len)
    Z_PARAM_OPTIONAL Z_PARAM_ZVAL(def)
        ZEND_PARSE_PARAMETERS_END();

    /* params 由 ControllerDispatcher 通过 request 属性 "routeParams" 传入 */
    controller_object_t *c = Z_CONTROLLER_P(getThis());
    if (Z_TYPE(c->request) == IS_UNDEF) {
        RETURN_NULL();
    }

    zval sv, ignored;
    ZVAL_STRINGL(&sv, key, key_len);
    zend_call_method_with_1_params(Z_OBJ(c->request), Z_OBJCE(c->request), NULL, "post", &ignored, &sv);
    zval_ptr_dtor(&sv);
    if (Z_TYPE(ignored) != IS_UNDEF || Z_TYPE(ignored) != IS_NULL) {
        RETURN_ZVAL(&ignored, 0, 0);
    }
    zval_ptr_dtor(&ignored);

    if (def)
        ZVAL_COPY(return_value, def);
    else
        RETURN_NULL();
}

/* ── arginfo ── */
ZEND_BEGIN_ARG_INFO_EX(ai_ctrl_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(ai_ctrl_view, 0, 0, 1)
ZEND_ARG_INFO(0, tpl)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(ai_ctrl_json, 0, 0, 1)
ZEND_ARG_INFO(0, data)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(ai_ctrl_text, 0, 0, 1)
ZEND_ARG_INFO(0, text)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(ai_ctrl_redir, 0, 0, 1)
ZEND_ARG_INFO(0, url)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(ai_ctrl_input, 0, 0, 1)
ZEND_ARG_INFO(0, key)
ZEND_END_ARG_INFO()

static const zend_function_entry nexo_controller_methods[] = {
    //
    PHP_ME(Controller, request, ai_ctrl_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, response, ai_ctrl_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, render, ai_ctrl_view, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, json, ai_ctrl_json, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, text, ai_ctrl_text, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, html, ai_ctrl_text, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, redirect, ai_ctrl_redir, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, input, ai_ctrl_input, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, param, ai_ctrl_input, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, params, ai_ctrl_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, headers, ai_ctrl_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Controller, post, ai_ctrl_input, ZEND_ACC_PUBLIC)
    //
    PHP_FE_END};

LUX_MINIT_FUNCTION(Controller) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\Controller", nexo_controller_methods);
    lux_controller_ce                = zend_register_internal_class(&ce);
    lux_controller_ce->create_object = controller_new;
    lux_controller_ce->ce_flags |= ZEND_ACC_EXPLICIT_ABSTRACT_CLASS;
    memcpy(&lux_controller_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_controller_handlers.offset   = XtOffsetOf(controller_object_t, std);
    lux_controller_handlers.free_obj = controller_free_obj;
    return SUCCESS;
}