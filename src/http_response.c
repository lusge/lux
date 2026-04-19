#include "http_response.h"
#include "http.h"
#include "php.h"
#include "standard/php_var.h"
#include "util/log.h"
#include "zend_API.h"
#include "zend_hash.h"
#include "zend_long.h"

#include "zend_smart_str.h"

#include "zend_types.h"
#include "zend_variables.h"

#include "ext/json/php_json.h"

#include "zend_types.h"
#include <string.h>

zend_class_entry           *lux_http_response_ce;
static zend_object_handlers lux_http_response_handlers;

// static zend_object *http_response_new(zend_class_entry *ce) {
//     http_response_object_t *obj = ecalloc(1, sizeof(http_response_object_t) + zend_object_properties_size(ce));
//     zend_object_std_init(&obj->std, ce);
//     obj->std.handlers = &http_response_handlers;
//     return &obj->std;
// }

static int parser_header_print(FIOBJ value, void *udata) {

    // 取当前循环的 key
    FIOBJ          key = fiobj_hash_key_in_loop();
    fio_str_info_s k   = fiobj_obj2cstr(key);

    // 标记，此值在对象结束后释放，所以不拷贝，试试出不出问题
    if (fiobj_type_is(value, FIOBJ_T_ARRAY)) {
        size_t len = fiobj_ary_count(value);
        for (int i = 0; i < len; i++) {
            FIOBJ          item = fiobj_ary_index(value, i);
            fio_str_info_s v    = fiobj_obj2cstr(item);
        }

    } else {
        fio_str_info_s v = fiobj_obj2cstr(value);
    }
    return 0;
}

// 打印 facil.io 的 HTTP 所有头 + Body
static void facil_http_dump_request(http_s *h) {
    if (!h)
        return;

    // 1. 打印基础信息
    LOG_INFO("\n=== HTTP Request Dump ===\n");
    LOG_INFO("METHOD: %s\n", fiobj_obj2cstr(h->method).data);
    LOG_INFO("PATH  : %s\n", fiobj_obj2cstr(h->path).data);
    LOG_INFO("QUERY : %s\n", fiobj_obj2cstr(h->query).data);

    // 2. 打印所有请求头
    LOG_INFO("\n--- HEADERS ---\n");

    fiobj_each1(h->headers, 0, parser_header_print, NULL);

    // 3. 打印 Body 长度 + 完整内容
    LOG_INFO("\n--- BODY ---\n");
    // LOG_INFO("Body length: %zu\n", h->body_len);
    if (h->body) {
        LOG_INFO("Body content: %s\n", fiobj_obj2cstr(h->body).data);
    } else {
        LOG_INFO("Body: empty\n");
    }

    LOG_INFO("=========================\n\n");
}

void build_response(zval *out) {
    object_init_ex(out, lux_http_response_ce);
    zval hdrs;
    array_init(&hdrs);
    zend_update_property(lux_http_response_ce, Z_OBJ_P(out), RESP_HEASERS, sizeof(RESP_HEASERS) - 1, &hdrs);
    zval_ptr_dtor(&hdrs);
}

void response_set_status_code(zval *resp_zv, int statusCode) {
    zend_update_property_long(lux_http_response_ce, Z_OBJ_P(resp_zv), RESP_STATUS_CODE, sizeof(RESP_STATUS_CODE) - 1, statusCode);
}

void response_set_body(zval *resp_zv, const char *body, size_t body_len) {
    zend_update_property_stringl(lux_http_response_ce, Z_OBJ_P(resp_zv), RESP_BODY, sizeof(RESP_BODY) - 1, body, body_len);
}

void send_response(http_s *req, zval *resp_zv) {
    zval *status_zv = zend_read_property(lux_http_response_ce, Z_OBJ_P(resp_zv),
                                         RESP_STATUS_CODE,
                                         sizeof(RESP_STATUS_CODE) - 1, 0, NULL);
    zval *body_zv   = zend_read_property(lux_http_response_ce, Z_OBJ_P(resp_zv),
                                         RESP_BODY,
                                         sizeof(RESP_BODY) - 1, 0, NULL);
    zval *hdrs_zv   = zend_read_property(lux_http_response_ce, Z_OBJ_P(resp_zv),
                                         RESP_HEASERS,
                                         sizeof(RESP_HEASERS) - 1, 0, NULL);

    req->status = (Z_TYPE_P(status_zv) == IS_LONG)
                      ? (int)Z_LVAL_P(status_zv)
                      : 200;

    if (Z_TYPE_P(hdrs_zv) == IS_ARRAY) {
        zend_string *key;
        zval        *hval;
        ZEND_HASH_FOREACH_STR_KEY_VAL(Z_ARRVAL_P(hdrs_zv), key, hval) {
            if (!key)
                continue;
            ZVAL_DEREF(hval);
            if (Z_TYPE_P(hval) != IS_STRING)
                continue;
            http_set_header2(req,
                             (fio_str_info_s){.data = ZSTR_VAL(key), .len = ZSTR_LEN(key)},
                             (fio_str_info_s){.data = Z_STRVAL_P(hval), .len = Z_STRLEN_P(hval)});
            // LOG_DEBUG("body_zv = %s \n", "aa");
        }
        ZEND_HASH_FOREACH_END();
    }

    // php_var_dump(hdrs_zv, 1);

    if (Z_TYPE_P(body_zv) == IS_STRING && Z_STRLEN_P(body_zv) > 0) {
        http_send_body(req, Z_STRVAL_P(body_zv), Z_STRLEN_P(body_zv));
    } else {
        http_send_body(req, "", 0);
    }
}

PHP_METHOD(HttpResponse, setStatus) {
    zend_long code;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_LONG(code)
    ZEND_PARSE_PARAMETERS_END();

    zend_update_property_long(lux_http_response_ce, Z_OBJ_P(getThis()), RESP_STATUS_CODE, sizeof(RESP_STATUS_CODE) - 1, code);
}

PHP_METHOD(HttpResponse, setHeader) {
    char  *key;
    size_t kl;
    char  *val;
    size_t vl;
    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_STRING(key, kl)
    Z_PARAM_STRING(val, vl)
    ZEND_PARSE_PARAMETERS_END();

    zval *hdrs = zend_read_property(lux_http_response_ce, Z_OBJ_P(getThis()), RESP_HEASERS, sizeof(RESP_HEASERS) - 1, 0, NULL);
    zval  zv;
    ZVAL_STRINGL(&zv, val, vl);
    add_assoc_zval(hdrs, key, &zv);
}

PHP_METHOD(HttpResponse, json) {
    zval *data;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_ARRAY(data)
    ZEND_PARSE_PARAMETERS_END();

    /* JSON 编码 */
    smart_str buf = {0};
    php_json_encode(&buf, data, 0);
    smart_str_0(&buf);

    zend_update_property_stringl(lux_http_response_ce, Z_OBJ_P(getThis()),
                                 RESP_BODY, sizeof(RESP_BODY) - 1,
                                 ZSTR_VAL(buf.s), ZSTR_LEN(buf.s));

    smart_str_free(&buf);
    zend_update_property_long(lux_http_response_ce, Z_OBJ_P(getThis()),
                              RESP_STATUS_CODE, sizeof(RESP_STATUS_CODE) - 1,
                              200);

    zval *hdrs = zend_read_property(lux_http_response_ce, Z_OBJ_P(getThis()),
                                    RESP_HEASERS, sizeof(RESP_HEASERS) - 1,
                                    0, NULL);
    ZVAL_DEREF(hdrs);

    if (Z_TYPE_P(hdrs) == IS_ARRAY) {
        add_assoc_string(hdrs, "Content-Type", "application/json; charset=utf-8");
    }
}

PHP_METHOD(HttpResponse, text) {
    char  *body;
    size_t body_len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(body, body_len)
    ZEND_PARSE_PARAMETERS_END();

    response_set_body(getThis(), body, body_len);
    zend_update_property_long(lux_http_response_ce, Z_OBJ_P(getThis()), RESP_STATUS_CODE, sizeof(RESP_STATUS_CODE) - 1, 200);
    zval *hdrs = zend_read_property(lux_http_response_ce, Z_OBJ_P(getThis()), RESP_HEASERS, sizeof(RESP_HEASERS) - 1, 0, NULL);

    add_assoc_string(hdrs, "Content-Type", "text/plain; charset=utf-8");
}
PHP_METHOD(HttpResponse, html) {

    char  *body;
    size_t body_len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(body, body_len)
    ZEND_PARSE_PARAMETERS_END();
    zend_update_property_stringl(lux_http_response_ce, Z_OBJ_P(getThis()), RESP_BODY, sizeof(RESP_BODY) - 1, body, body_len);
    zend_update_property_long(lux_http_response_ce, Z_OBJ_P(getThis()), RESP_STATUS_CODE, sizeof(RESP_STATUS_CODE) - 1, 200);

    zval *hdrs = zend_read_property(lux_http_response_ce, Z_OBJ_P(getThis()), RESP_HEASERS, sizeof(RESP_HEASERS) - 1, 0, NULL);

    add_assoc_string(hdrs, "Content-Type", "text/html; charset=utf-8");
}

ZEND_BEGIN_ARG_INFO_EX(hr_set_header, 0, 0, 2)
ZEND_ARG_INFO(0, path)
ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(hr_ret, 0, 0, 1)
ZEND_ARG_INFO(0, body)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(hr_json, 0, 0, 1)
ZEND_ARG_INFO(0, arr)
ZEND_END_ARG_INFO()

static const zend_function_entry http_response_method[] = {
    PHP_ME(HttpResponse, setStatus, hr_ret, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpResponse, setHeader, hr_set_header, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpResponse, json, hr_json, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpResponse, text, hr_ret, ZEND_ACC_PUBLIC)
    //
    PHP_ME(HttpResponse, html, hr_ret, ZEND_ACC_PUBLIC)
    //
    PHP_FE_END};

LUX_MINIT_FUNCTION(HttpResponse) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\Http\\Response", http_response_method);
    lux_http_response_ce = zend_register_internal_class(&ce);
    // http_response_ce->create_object = http_response_new;
    memcpy(&lux_http_response_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));

    zend_declare_property_long(lux_http_response_ce, RESP_STATUS_CODE, sizeof(RESP_STATUS_CODE) - 1, 200, ZEND_ACC_PUBLIC);
    zend_declare_property_string(lux_http_response_ce, RESP_BODY, sizeof(RESP_BODY) - 1, "", ZEND_ACC_PUBLIC);
    zend_declare_property_null(lux_http_response_ce, RESP_HEASERS, sizeof(RESP_HEASERS) - 1, ZEND_ACC_PUBLIC);

    return SUCCESS;
}