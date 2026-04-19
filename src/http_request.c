#include "http_request.h"
#include "fio.h"
#include "fiobj_ary.h"
#include "fiobject.h"
#include "http.h"
#include "php.h"
#include "php_lux.h"
#include "standard/php_var.h"
#include "util/log.h"
#include "zend_API.h"
#include "zend_hash.h"
#include "zend_types.h"
#include "zend_variables.h"
#include <string.h>

zend_class_entry           *lux_http_request_ce;
static zend_object_handlers lux_http_request_handlers;

static void http_request_free_obj(zend_object *o) {
    http_request_object_t *c = (http_request_object_t *)((char *)o - XtOffsetOf(http_request_object_t, std));
    zend_object_std_dtor(o);
}

static zend_object *http_request_new(zend_class_entry *ce) {
    http_request_object_t *c = ecalloc(1, sizeof(http_request_object_t) + zend_object_properties_size(ce));
    zend_object_std_init(&c->std, ce);
    object_properties_init(&c->std, ce);
    c->std.handlers = &lux_http_request_handlers;
    return &c->std;
}

static int hasmap_to_array(FIOBJ value, void *udata) {
    zval *array = (zval *)udata;
    // 取当前循环的 key
    FIOBJ          key = fiobj_hash_key_in_loop();
    fio_str_info_s k   = fiobj_obj2cstr(key);

    // 标记，此值在对象结束后释放，所以不拷贝，试试出不出问题
    if (fiobj_type_is(value, FIOBJ_T_ARRAY)) {
        zval sarr;
        array_init(&sarr);
        size_t len = fiobj_ary_count(value);
        for (int i = 0; i < len; i++) {
            FIOBJ          item = fiobj_ary_index(value, i);
            fio_str_info_s v    = fiobj_obj2cstr(item);
            add_index_string(&sarr, i, estrdup(v.data));
        }
        add_assoc_zval(array, estrdup(k.data), &sarr);
        zval_ptr_dtor(&sarr);
    } else {
        fio_str_info_s v = fiobj_obj2cstr(value);
        add_assoc_string(array, estrdup(k.data), estrdup(v.data));
    }

    return 0;
}

void build_request(zval *out, http_s *req, zval *params) {
    object_init_ex(out, lux_http_request_ce);

    http_request_object_t *obj = Z_HTTP_REQUEST_P(out);
    obj->req                   = req;

    http_parse_query(req);

    fio_str_info_s method = fiobj_obj2cstr(obj->req->method);
    if (memcmp(method.data, "GET", 1) == 0) {
        http_parse_body(obj->req);
    }
    // params
    if (Z_TYPE_P(params) != IS_UNDEF) {
        zend_update_property(lux_http_request_ce, Z_OBJ_P(out), "params", sizeof("params") - 1, params);
    }
}

PHP_METHOD(HttpRequest, getMethod) {
    http_request_object_t *obj    = Z_HTTP_REQUEST_P(getThis());
    fio_str_info_s         method = fiobj_obj2cstr(obj->req->method);
    RETURN_STRINGL(method.data, method.len);
}

PHP_METHOD(HttpRequest, getPath) {
    http_request_object_t *obj  = Z_HTTP_REQUEST_P(getThis());
    fio_str_info_s         path = fiobj_obj2cstr(obj->req->path);
    RETURN_STRINGL(path.data, path.len);
}

PHP_METHOD(HttpRequest, getQuery) {
    http_request_object_t *obj   = Z_HTTP_REQUEST_P(getThis());
    fio_str_info_s         query = fiobj_obj2cstr(obj->req->query);
    RETURN_STRINGL(query.data, query.len);
}

PHP_METHOD(HttpRequest, getBody) {
    http_request_object_t *obj  = Z_HTTP_REQUEST_P(getThis());
    fio_str_info_s         body = fiobj_obj2cstr(obj->req->body);
    RETURN_STRINGL(body.data, body.len);
}

PHP_METHOD(HttpRequest, getRemoteIp) {
    http_request_object_t *obj = Z_HTTP_REQUEST_P(getThis());
    fio_str_info_s         ip  = http_peer_addr(obj->req);
    RETURN_STRINGL(ip.data, ip.len);
}

PHP_METHOD(HttpRequest, getHeader) {
    char  *name;
    size_t name_len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();

    http_request_object_t *obj = Z_HTTP_REQUEST_P(getThis());

    FIOBJ key = fiobj_str_new(name, name_len);
    FIOBJ val = fiobj_hash_get(obj->req->headers, key);
    fiobj_free(key);

    if (val) {
        fio_str_info_s s = fiobj_obj2cstr(val);
        RETURN_STRINGL(s.data, s.len);
    }

    RETURN_NULL();
}

PHP_METHOD(HttpRequest, getParams) {
    RETURN_ZVAL(zend_read_property(lux_http_request_ce, Z_OBJ_P(getThis()), "params", sizeof("params") - 1, 0, NULL), 1, 0);
}

PHP_METHOD(HttpRequest, getParam) {
    char  *name;
    size_t name_len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();

    zval *params = zend_read_property(lux_http_request_ce, Z_OBJ_P(getThis()), "params", sizeof("params") - 1, 0, NULL);

    zval *zv = zend_hash_str_find(Z_ARRVAL_P(params), name, name_len);

    RETURN_ZVAL(zv, 1, 0);
}

PHP_METHOD(HttpRequest, post) {
    char  *name;
    size_t name_len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(name, name_len)
    ZEND_PARSE_PARAMETERS_END();

    http_request_object_t *obj = Z_HTTP_REQUEST_P(getThis());

    FIOBJ key = fiobj_str_new(name, name_len);
    FIOBJ val = fiobj_hash_get(obj->req->params, key);
    fiobj_free(key);

    if (val) {
        fio_str_info_s s = fiobj_obj2cstr(val);

        RETURN_STRINGL(s.data, s.len);
    }

    RETURN_NULL();
}

// 接受json,
PHP_METHOD(HttpRequest, input) {
    http_request_object_t *obj    = Z_HTTP_REQUEST_P(getThis());
    FIOBJ                  ct_val = fiobj_hash_get(obj->req->headers, HTTP_HEADER_CONTENT_TYPE);
    fio_str_info_s         ct     = ct_val ? fiobj_obj2cstr(ct_val) : (fio_str_info_s){0};

    if (ct.len >= 16 && memcmp(ct.data, "application/json", 16) == 0) {
    }

    fio_str_info_s raw   = fiobj_obj2cstr(obj->req->body);
    FIOBJ          jsObj = FIOBJ_INVALID;
    if (!fiobj_json2obj(&jsObj, raw.data, raw.len)) {
        RETURN_NULL();
    }

    zval arr;
    array_init(&arr);

    fiobj_each1(jsObj, 0, hasmap_to_array, &arr);

    RETURN_ZVAL(&arr, 0, 0);
}

ZEND_BEGIN_ARG_INFO_EX(req_void, 0, 0, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(req_str, 0, 0, 1)
ZEND_ARG_INFO(0, name)
ZEND_END_ARG_INFO()

static const zend_function_entry http_request_methods[] = {
    //
    PHP_ME(HttpRequest, getMethod, req_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, getPath, req_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, getQuery, req_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, getBody, req_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, getRemoteIp, req_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, getHeader, req_str, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, getParams, req_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, getParam, req_str, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, input, req_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpRequest, post, req_str, ZEND_ACC_PUBLIC)
    //
    PHP_FE_END};

LUX_MINIT_FUNCTION(HttpRequest) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\Http\\Request", http_request_methods);
    lux_http_request_ce                = zend_register_internal_class(&ce);
    lux_http_request_ce->create_object = http_request_new;
    memcpy(&lux_http_request_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_http_request_handlers.offset   = XtOffsetOf(http_request_object_t, std);
    lux_http_request_handlers.free_obj = http_request_free_obj;

    zend_declare_property_null(lux_http_request_ce, "params", sizeof("params") - 1, ZEND_ACC_PUBLIC);
    return SUCCESS;
}