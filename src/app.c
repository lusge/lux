#include "app.h"
#include "http.h"
#include "php.h"
#include "standard/php_var.h"
#include "util/log.h"
#include "util/php_cb.h"
#include "zend_API.h"
#include "zend_alloc.h"
#include "zend_compile.h"
#include "zend_interfaces.h"
#include "zend_types.h"
#include "zend_variables.h"

#include "ctrl_dispatcher.h"
#include "http_request.h"
#include "http_response.h"
#include "http_websockets.h"
#include "router.h"
#include "view.h"

zend_class_entry           *lux_app_ce;
static zend_object_handlers lux_app_handlers;

static zend_object *app_class_new(zend_class_entry *ce) {
    app_object_t *app = ecalloc(1, sizeof(app_object_t) + zend_object_properties_size(ce));
    zend_object_std_init(&app->std, ce);
    app->std.handlers = &lux_app_handlers;

    ZVAL_UNDEF(&app->router);
    ZVAL_UNDEF(&app->view);
    return &app->std;
}

static void app_class_free_obj(zend_object *obj) {
    app_object_t *app = Z_APP_P(obj);
    zval_ptr_dtor(&app->router);
    zval_ptr_dtor(&app->view);

    // if (app->view_path) {
    //     efree(app->view_path);
    // }
    zend_object_std_dtor(&app->std);
}

static int is_static_file(const char *path, size_t len) {
    for (size_t i = len; i > 0; i--) {
        if (path[i - 1] == '/')
            break; // 遇到路径分隔符，没有扩展名
        if (path[i - 1] == '.')
            return 1; // 有扩展名，是静态文件
    }
    return 0;
}

// 原来的代码已移到框架部分
static void http_request_cb(http_s *req) {

    app_object_t  *app  = (app_object_t *)req->udata;
    fio_str_info_s path = fiobj_obj2cstr(req->path);

    // 不接受静态文件请求，交给nginx
    if (is_static_file(path.data, path.len)) {
        http_send_error(req, 404);
        return;
    }

    zval request_zv, response_zv, params;
    build_response(&response_zv);
    ZVAL_UNDEF(&params);

    // 将mvc中的路由替换http老版本的路由。

    // 当前的路由
    route_entry_t *route = router_match(&app->router, fiobj_obj2cstr(req->method).data,
                                        path.data, &params);

    php_cb_t *route_cb = NULL;
    if (route) {
        if (route->is_controller) {
            zval handler_zv;
            make_dispatcher(&handler_zv, route->controller_class, route->controller_method, Z_TYPE(app->view) != IS_UNDEF ? &app->view : NULL);
            route_cb = php_cb_new(&handler_zv);
        } else {
            route_cb = route->callback;
        }
    } else {
        php_cb_t *ncb = router_get_not_fount_cb(&app->router);
        if (ncb != NULL) {
            route_cb = ncb;
        } else {
            response_set_status_code(&response_zv, 404);
            response_set_body(&response_zv, "404 Not Found", sizeof("404 Not Found") - 1);
        }
    }

    build_request(&request_zv, req, &params);
    if (route_cb) {
        middleware_run(route->middlewares, &request_zv, &response_zv, route_cb);
    }

    send_response(req, &response_zv);

    zval_ptr_dtor(&request_zv);
    zval_ptr_dtor(&response_zv);
}

static void on_upgrade_cb(http_s *req, char *target, size_t len) {
    fio_str_info_s path = fiobj_obj2cstr(req->path);

    // 只接受 websocket 升级
    if (len != 9 || memcmp(target, "websocket", 9)) {
        http_send_error(req, 400);
        return;
    }

    app_object_t *app      = (app_object_t *)req->udata;
    ws_entry_t   *ws_route = router_ws_match(&app->router, path.data);

    if (ws_route) {
        http_upgrade2ws(req,
                        .on_open    = ws_on_open,
                        .on_message = ws_on_message,
                        .on_close   = ws_on_close,
                        .udata      = ws_route);
        return;
    }

    http_send_error(req, 404);
}

PHP_METHOD(App, __construct) {
    char     *host;
    size_t    host_len;
    zend_long workers       = 4;
    char     *view_path     = NULL;
    size_t    view_path_len = 0;
    zend_long port;

    ZEND_PARSE_PARAMETERS_START(2, 4)
    Z_PARAM_STRING(host, host_len)
    Z_PARAM_LONG(port)
    Z_PARAM_OPTIONAL
    Z_PARAM_LONG(workers)
    Z_PARAM_STRING(view_path, view_path_len)
    ZEND_PARSE_PARAMETERS_END();

    app_object_t *app = Z_APP_OBJ_P(getThis());
    app->host         = estrndup(host, host_len);
    app->port         = port;
    app->num_workers  = (int)workers > 0 ? (int)workers : 4;

    object_init_ex(&app->router, lux_router_ce);

    if (view_path && view_path_len > 0) {

        object_init_ex(&app->view, lux_view_ce);
        zval vp;
        ZVAL_STRINGL(&vp, view_path, view_path_len);

        zend_call_method_with_1_params(Z_OBJ(app->view), lux_view_ce, NULL, "__construct", NULL, &vp);
        zval_ptr_dtor(&vp);
        // app->view_path = estrndup(view_path, view_path_len);
    }
}

PHP_METHOD(App, router) {
    app_object_t *app = Z_APP_OBJ_P(getThis());

    ZVAL_COPY(return_value, &app->router);
}

PHP_METHOD(App, view) {
    app_object_t *app = Z_APP_OBJ_P(getThis());

    ZVAL_COPY(return_value, &app->view);
}

PHP_METHOD(App, start) {
    app_object_t *app = Z_APP_OBJ_P(getThis());

    if (!app->host) {
        zend_throw_exception(NULL, "HttpServer: address not set", 0);
        return;
    }

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", (int)app->port);
    http_listen(port_str, app->host,
                .on_request = http_request_cb,
                .on_upgrade = on_upgrade_cb,
                .udata      = app, );

    fio_start(.threads = 1, .workers = app->num_workers);
}

PHP_METHOD(App, run) {
    zend_call_method_with_0_params(Z_OBJ_P(getThis()), Z_OBJCE_P(getThis()), NULL, "start", return_value);
}

ZEND_BEGIN_ARG_INFO_EX(app_ctor, 0, 0, 2)
ZEND_ARG_INFO(0, host)
ZEND_ARG_INFO(0, port)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(app_void, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry app_methods[] = {
    PHP_ME(App, __construct, app_ctor, ZEND_ACC_PUBLIC | ZEND_ACC_CTOR)
    //
    PHP_ME(App, router, app_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(App, view, app_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(App, start, app_void, ZEND_ACC_PUBLIC)
    //
    PHP_ME(App, run, app_void, ZEND_ACC_PUBLIC)
    //
    PHP_FE_END
    //
};

LUX_MINIT_FUNCTION(App) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\App", app_methods);
    lux_app_ce = zend_register_internal_class(&ce);

    memcpy(&lux_app_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_app_handlers.offset   = XtOffsetOf(app_object_t, std);
    lux_app_handlers.free_obj = app_class_free_obj;

    return SUCCESS;
}