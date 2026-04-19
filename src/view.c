#include "view.h"
#include "util/log.h"

#include "Zend/zend_string.h"
#include "main/php_main.h"

zend_class_entry           *lux_view_ce;
static zend_object_handlers lux_view_handlers;

static void view_free_obj(zend_object *o) {
    view_object_t *obj = (view_object_t *)((char *)o - XtOffsetOf(view_object_t, std));
    if (obj->view_path) {
        efree(obj->view_path);
    }

    if (obj->layout) {
        efree(obj->layout);
    }

    zend_object_std_dtor(o);
}

static zend_object *view_new(zend_class_entry *ce) {
    view_object_t *obj = ecalloc(1, sizeof(view_object_t) + zend_object_properties_size(ce));
    zend_object_std_init(&obj->std, ce);
    obj->std.handlers = &lux_view_handlers;
    return &obj->std;
}

static zend_string *render_file(const char *path, zval *data) {
    zval         retval;
    zend_string *output = NULL;
    if (php_output_start_user(NULL, 0, PHP_OUTPUT_HANDLER_STDFLAGS) == FAILURE) {
        zend_throw_exception_ex(NULL, 0, "View: ob_start() failed for: %s", path);
        return NULL;
    }

    if (data && Z_TYPE_P(data) == IS_ARRAY) {
        zend_hash_merge(
            EG(current_execute_data)
                ? zend_rebuild_symbol_table()
                : &EG(symbol_table),
            Z_ARRVAL_P(data),
            zval_add_ref, /* copy */
            0             /* 不覆盖已有变量 → 改为 1 则覆盖 */
        );
    }

    zend_file_handle file_handle;
    zend_stream_init_filename(&file_handle, path);

    int ret = php_execute_script(&file_handle);

    if (php_output_get_contents(&retval) == SUCCESS) {
        output = zval_get_string(&retval);
        zval_ptr_dtor(&retval);
    }
    php_output_discard();

    if (ret == FAILURE && !output) {
        zend_throw_exception_ex(NULL, 0, "View: failed to execute: %s", path);
        return NULL;
    }

    return output ? output : ZSTR_EMPTY_ALLOC();
}

PHP_METHOD(View, __construct) {
    char  *path;
    size_t path_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(path, path_len)
    ZEND_PARSE_PARAMETERS_END();

    view_object_t *obj = Z_VIEW_P(getThis());
    obj->view_path     = estrndup(path, path_len);
    // php_var_dump(&vp, 1);
}

PHP_METHOD(View, setLayout) {
    char  *layout;
    size_t layout_len;

    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(layout, layout_len)
    ZEND_PARSE_PARAMETERS_END();

    view_object_t *obj = Z_VIEW_P(getThis());
    obj->layout        = estrndup(layout, layout_len);
}

PHP_METHOD(View, render) {
    char  *tpl;
    size_t tpl_len;
    zval  *data = NULL;

    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(tpl, tpl_len)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY_OR_NULL(data)
    ZEND_PARSE_PARAMETERS_END();

    view_object_t *obj = Z_VIEW_P(getThis());
    // LOG_INFO("view_path = %s  layout = %s", obj->view_path, obj->layout);

    char path[4096];
    snprintf(path, sizeof(path), "%s/%s.php", obj->view_path, tpl); // 拼接文件路径

    struct stat st;
    if (stat(path, &st) != 0) {
        zend_throw_exception_ex(lux_view_ce, 0, "View: template not found: %s", path);
        return;
    }

    zend_string *content = render_file(path, data);

    if (!content)
        return;

    if (obj->layout[0]) {
        char layout_path[4096];
        snprintf(layout_path, sizeof(layout_path),
                 "%s/%s.php", obj->view_path, obj->layout);

        if (stat(layout_path, &st) != 0) {
            zend_string_release(content);
            zend_throw_exception_ex(NULL, 0,
                                    "View: layout not found: %s", layout_path);
            return;
        }

        /* 把 content 字符串注入 $data，传给 layout */
        zval data_with_content;
        if (data && Z_TYPE_P(data) == IS_ARRAY) {
            /* 复制原 data 数组，追加 content */
            ZVAL_ARR(&data_with_content,
                     zend_array_dup(Z_ARRVAL_P(data)));
        } else {
            array_init(&data_with_content);
        }

        zval content_zv;
        ZVAL_STR(&content_zv, content); /* 转移所有权给数组 */
        add_assoc_zval(&data_with_content, "content", &content_zv);

        zend_string *final = render_file(layout_path, &data_with_content);
        zval_ptr_dtor(&data_with_content); /* 释放数组（含 content 字符串）*/

        if (!final)
            return;

        RETURN_STR(final);
    }

    RETURN_STR(content);
}

PHP_METHOD(View, exists) {
    char  *tpl;
    size_t tpl_len;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(tpl, tpl_len)
    ZEND_PARSE_PARAMETERS_END();

    view_object_t *obj = Z_VIEW_P(getThis());
    char           path[4096];
    snprintf(path, sizeof(path), "%s/%s.php", obj->view_path, tpl);

    struct stat st;
    RETURN_BOOL(stat(path, &st) == 0);
}

ZEND_BEGIN_ARG_INFO_EX(arginfo_view_ctor, 0, 0, 1)
ZEND_ARG_TYPE_INFO(0, path, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_view_render, 0, 1, IS_STRING, 0)
ZEND_ARG_TYPE_INFO(0, template, IS_STRING, 0)
ZEND_ARG_TYPE_INFO_WITH_DEFAULT_VALUE(0, data, IS_ARRAY, 1, "null")
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_view_set_layout, 0, 1, IS_STATIC, 0)
ZEND_ARG_TYPE_INFO(0, layout, IS_STRING, 0)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_WITH_RETURN_TYPE_INFO_EX(arginfo_view_exists, 0, 1, _IS_BOOL, 0)
ZEND_ARG_TYPE_INFO(0, template, IS_STRING, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry view_methods[] = {
    PHP_ME(View, __construct, arginfo_view_ctor, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    //
    PHP_ME(View, setLayout, arginfo_view_set_layout, ZEND_ACC_PUBLIC)
    //
    PHP_ME(View, render, arginfo_view_render, ZEND_ACC_PUBLIC)
    //
    PHP_ME(View, exists, arginfo_view_exists, ZEND_ACC_PUBLIC)
    //
    PHP_FE_END
    //
};

LUX_MINIT_FUNCTION(View) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\View", view_methods);
    lux_view_ce                = zend_register_internal_class(&ce);
    lux_view_ce->create_object = view_new;
    memcpy(&lux_view_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_view_handlers.offset   = XtOffsetOf(view_object_t, std);
    lux_view_handlers.free_obj = view_free_obj;

    return SUCCESS;
}