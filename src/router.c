#include "router.h"
#include "php.h"
#include "standard/php_var.h"
#include "util/log.h"
#include "util/php_cb.h"
#include "zend_API.h"
#include "zend_alloc.h"
#include "zend_compile.h"
#include "zend_hash.h"
#include "zend_operators.h"
#include "zend_smart_str.h"
#include "zend_smart_str_public.h"
#include "zend_string.h"
#include "zend_types.h"
#include "zend_variables.h"
#include <stddef.h>
#include <string.h>

zend_class_entry           *lux_router_ce;
static zend_object_handlers lux_router_handlers;

zend_class_entry           *lux_mw_next_ce;
static zend_object_handlers lux_mw_next_handlers;

static zend_object *router_class_new(zend_class_entry *ce) {
    router_object_t *router = ecalloc(1, sizeof(router_object_t) + zend_object_properties_size(ce));
    zend_object_std_init(&router->std, ce);
    router->std.handlers = &lux_router_handlers;

    return &router->std;
}

static void router_class_free_obj(zend_object *obj) {
    router_object_t *router = (router_object_t *)((char *)obj - XtOffsetOf(router_object_t, std));

    for (route_entry_t *e = router->routes; e;) {
        route_entry_t *next = e->next;
        efree(e->path);
        if (e->controller_class) {
            efree(e->controller_class);
        }

        if (e->controller_method) {
            efree(e->controller_method);
        }

        if (e->name) {
            efree(e->name);
        }

        if (!e->is_controller) {
            php_cb_free(e->callback);
        }

        for (route_mw_entry_t *m = e->middlewares; m;) {
            route_mw_entry_t *mn = m->next;
            php_cb_free(m->callback);
            efree(m);
            m = mn;
        }
        efree(e);
        e = next;
    }

    for (ws_entry_t *e = router->ws_routes; e;) {
        ws_entry_t *next = e->next;
        efree(e->path);
        php_cb_free(e->on_close);
        php_cb_free(e->on_message);
        php_cb_free(e->on_open);
        _efree(e);
        e = next;
    }

    for (route_group_t *g = router->group_stack; g;) {
        route_group_t *gn = g->next;
        efree(g->prefix);
        efree(g);
        g = gn;
    }

    php_cb_free(router->not_found_cb);
    zend_object_std_dtor(&router->std);
}

static char *build_path(router_object_t *r, const char *path) {
    smart_str buf = {0};
    for (route_group_t *g = r->group_stack; g; g = g->next) {
        smart_str_appends(&buf, g->prefix);
    }
    smart_str_appends(&buf, path);
    smart_str_0(&buf);
    char *result = estrdup(ZSTR_VAL(buf.s));
    smart_str_free(&buf);
    return result;
}

static route_entry_t *add_router(router_object_t *r, const char *http_method, const char *path, int is_ctrl, const char *ctrl_cls, const char *ctrl_mth, zval *cb) {
    route_entry_t *e = ecalloc(1, sizeof(route_entry_t));
    e->method        = method_to_id(http_method);
    e->path          = build_path(r, path);
    e->is_controller = is_ctrl;

    if (is_ctrl) {
        e->controller_class  = estrdup(ctrl_cls);
        e->controller_method = estrdup(ctrl_mth);
    } else {
        e->callback = php_cb_new(cb);
    }

    if (!r->routes) {
        r->routes = r->routes_tail = e;
    } else {
        r->routes_tail->next = e;
        r->routes_tail       = e;
    }

    return e;
}

// 简单的 URL decode，写到 out，返回实际长度
static size_t url_decode(const char *src, size_t slen, char *out, size_t olen) {
    size_t j = 0;
    for (size_t i = 0; i < slen && j < olen - 1; i++) {
        if (src[i] == '%' && i + 2 < slen) {
            char          hex[3] = {src[i + 1], src[i + 2], '\0'};
            char         *end;
            unsigned long v = strtoul(hex, &end, 16);
            if (end == hex + 2) {
                out[j++] = (char)v;
                i += 2;
                continue;
            }
        }
        out[j++] = (src[i] == '+') ? ' ' : src[i];
    }
    out[j] = '\0';
    return j;
}

typedef struct {
    char   key[128];
    char   val[1024];
    size_t val_len;
} param_t;

route_entry_t *router_match(zval *router_zv, const char *method, const char *path, zval *params_out) {
    char   norm[2048];
    size_t plen = strlen(path);
    if (plen > 1 && path[plen - 1] == '/')
        plen--;
    memcpy(norm, path, plen);
    norm[plen] = '\0';
    path       = norm;

    router_object_t *obj = Z_ROUTER_P(router_zv);

    for (route_entry_t *r = obj->routes; r; r = r->next) {
        if (r->method != method_to_id(method)) { // strcmp(r->method, method) != 0) {
            continue;
        }

        // 直接匹配
        if (strcmp(r->path, path) == 0) {
            array_init(params_out);
            return r;
        }

        param_t     params[32];
        int         param_cnt = 0;
        const char *rp = r->path, *pp = path;
        int         matched = 1;

        while (*rp && *pp) {
            if (*rp == ':') {
                const char *ks = ++rp;
                while (*rp && *rp != '/') {
                    rp++;
                }

                size_t kl = (size_t)(rp - ks);

                if (kl >= sizeof(params[0].key) || param_cnt >= 32) {
                    matched = 0;
                    break;
                }

                memcpy(params[param_cnt].key, ks, kl);
                params[param_cnt].key[kl] = '\0';

                // value
                const char *vs = pp;
                while (*pp && *pp != '/') {
                    pp++;
                }

                params[param_cnt].val_len = url_decode(vs, (size_t)(pp - vs), params[param_cnt].val, sizeof(params[param_cnt].val));
                param_cnt++;
            } else if (*rp == *pp) {
                rp++;
                pp++;
            } else {
                matched = 0;
                break;
            }
        }

        if (!matched || *rp != '\0' || *pp != '\0') {
            continue;
        }

        array_init(params_out);
        for (int i = 0; i < param_cnt; i++) {
            add_assoc_stringl(params_out, params[i].key, params[i].val, params[i].val_len);
        }

        return r;
    }

    return NULL;
}

php_cb_t *router_get_not_fount_cb(zval *router_zv) {
    router_object_t *obj = Z_ROUTER_P(router_zv);

    return obj->not_found_cb;
}

ws_entry_t *router_ws_match(zval *router_zv, const char *path) {
    router_object_t *obj = Z_ROUTER_P(router_zv);

    for (ws_entry_t *r = obj->ws_routes; r; r = r->next)
        if (strcmp(r->path, path) == 0) {
            return r;
        }

    return NULL;
}

static void register_route(INTERNAL_FUNCTION_PARAMETERS, const char *http_method) {
    char  *path;
    size_t path_len;
    zval  *handler;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_STRING(path, path_len)
    Z_PARAM_ZVAL(handler)
    ZEND_PARSE_PARAMETERS_END();

    router_object_t *r = Z_ROUTER_P(getThis());

    if (Z_TYPE_P(handler) == IS_STRING) {
        char *at = strchr(Z_STRVAL_P(handler), '@');

        if (at) {
            size_t clen = (size_t)(at - Z_STRVAL_P(handler));
            size_t mlen = Z_STRLEN_P(handler) - clen - 1; /* @ 后面的长度 */

            /* 直接用 estrndup，不需要临时缓冲区 */
            char *cls = estrndup(Z_STRVAL_P(handler), clen);
            char *mth = estrndup(at + 1, mlen);
            add_router(r, http_method, path, 1, cls, mth, NULL);
            efree(cls);
            efree(mth);
        } else {
            zend_throw_exception_ex(NULL, 0, "Invalid handler '%s', expected 'Class@method'", Z_STRVAL_P(handler));
        }
    } else {
        add_router(r, http_method, path, 0, NULL, NULL, handler);
    }

    RETURN_OBJ_COPY(Z_OBJ_P(getThis()));
}

PHP_METHOD(Router, get) { register_route(INTERNAL_FUNCTION_PARAM_PASSTHRU, "GET"); }
PHP_METHOD(Router, post) { register_route(INTERNAL_FUNCTION_PARAM_PASSTHRU, "POST"); }
PHP_METHOD(Router, put) { register_route(INTERNAL_FUNCTION_PARAM_PASSTHRU, "PUT"); }
PHP_METHOD(Router, delete) { register_route(INTERNAL_FUNCTION_PARAM_PASSTHRU, "DELETE"); }
PHP_METHOD(Router, patch) { register_route(INTERNAL_FUNCTION_PARAM_PASSTHRU, "PATCH"); }

PHP_METHOD(Router, group) {
    char  *perfix;
    size_t perfix_len;
    zval  *cb;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_STRING(perfix, perfix_len)
    Z_PARAM_ZVAL(cb)
    ZEND_PARSE_PARAMETERS_END();

    router_object_t *r = Z_ROUTER_P(getThis());

    route_group_t *g = emalloc(sizeof(route_group_t));
    g->prefix        = estrndup(perfix, perfix_len);
    g->next          = NULL;

    // 改为尾部插入
    if (!r->group_stack) {
        r->group_stack = g;
    } else {
        route_group_t *current = r->group_stack;
        while (current->next) {
            current = current->next;
        }
        current->next = g;
    }

    zval retval, self_zv;
    ZVAL_OBJ(&self_zv, &r->std);
    zend_fcall_info       fci = empty_fcall_info;
    zend_fcall_info_cache fcc = empty_fcall_info_cache;
    if (zend_fcall_info_init(cb, 0, &fci, &fcc, NULL, NULL) == SUCCESS) {
        fci.retval      = &retval;
        fci.params      = &self_zv;
        fci.param_count = 1;
        zend_call_function(&fci, &fcc);
        zval_ptr_dtor(&retval);
    }

    // 移除当前添加的组
    if (r->group_stack == g) {
        r->group_stack = g->next;
    } else {
        route_group_t *current = r->group_stack;
        while (current && current->next != g) {
            current = current->next;
        }
        if (current) {
            current->next = g->next;
        }
    }

    efree(g->prefix);
    efree(g);
    RETURN_OBJ_COPY(Z_OBJ_P(getThis()));
}

PHP_METHOD(Router, ws) {
    char  *path;
    size_t path_len;
    zval  *config;

    ZEND_PARSE_PARAMETERS_START(2, 2)
    Z_PARAM_STRING(path, path_len)
    Z_PARAM_ARRAY(config)
    ZEND_PARSE_PARAMETERS_END();

    router_object_t *r = Z_ROUTER_P(getThis());

    ws_entry_t *e = ecalloc(1, sizeof(ws_entry_t));
    e->path       = build_path(r, path);

    HashTable *ht = Z_ARRVAL_P(config);
    zval      *cb;
#define LOAD_WS(key, field)                                                                   \
    if ((cb = zend_hash_str_find(ht, key, sizeof(key) - 1)) && zend_is_callable(cb, 0, NULL)) \
        e->field = php_cb_new(cb);
    LOAD_WS("open", on_open)
    LOAD_WS("message", on_message)
    LOAD_WS("close", on_close)
#undef LOAD_WS

    e->next      = r->ws_routes;
    r->ws_routes = e;
    RETURN_OBJ_COPY(Z_OBJ_P(getThis()));
}

PHP_METHOD(Router, notFound) {
    zval *cb;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_ZVAL(cb)
    ZEND_PARSE_PARAMETERS_END();

    router_object_t *r = Z_ROUTER_P(getThis());

    php_cb_free(r->not_found_cb);
    r->not_found_cb = php_cb_new(cb);
    RETURN_OBJ_COPY(Z_OBJ_P(getThis()));
}

PHP_METHOD(Router, name) {
    char  *n;
    size_t nl;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_STRING(n, nl)
    ZEND_PARSE_PARAMETERS_END();

    router_object_t *r = Z_ROUTER_P(getThis());
    if (r->routes_tail) {
        if (r->routes_tail->name)
            efree(r->routes_tail->name);
        r->routes_tail->name = estrndup(n, nl);
    }

    RETURN_OBJ_COPY(Z_OBJ_P(getThis()));
}

PHP_METHOD(Router, middleware) {
    zval *cb;
    ZEND_PARSE_PARAMETERS_START(1, 1)
    Z_PARAM_ZVAL(cb)
    ZEND_PARSE_PARAMETERS_END();
    router_object_t *r = Z_ROUTER_P(getThis());
    if (r->routes_tail) {
        route_mw_entry_t *m         = emalloc(sizeof(route_mw_entry_t));
        m->callback                 = php_cb_new(cb);
        m->next                     = r->routes_tail->middlewares;
        r->routes_tail->middlewares = m;
    }
    RETURN_OBJ_COPY(Z_OBJ_P(getThis()));
}

// 反向生成URL
PHP_METHOD(Router, url) {
    char  *name;
    size_t nl;
    zval  *params = NULL;
    ZEND_PARSE_PARAMETERS_START(1, 2)
    Z_PARAM_STRING(name, nl)
    Z_PARAM_OPTIONAL
    Z_PARAM_ARRAY(params)
    ZEND_PARSE_PARAMETERS_END();

    router_object_t *r     = Z_ROUTER_P(getThis());
    route_entry_t   *found = NULL;

    for (route_entry_t *e = r->routes; e; e = e->next) {
        if (e->name && strcmp(e->name, name) == 0) {
            found = e;
            break;
        }
    }

    if (!found) {
        RETURN_NULL();
    }

    smart_str   url = {0};
    const char *p   = found->path;
    while (*p) {
        if (*p == ':' && params && Z_TYPE_P(params) == IS_ARRAY) {
            p++;
            const char *ks = p;
            while (*p && *p != '/') {
                p++;
            }
            char key[128] = {0};
            memcpy(key, ks, p - ks < 127 ? p - ks : 127);
            zval *val = zend_hash_str_find(Z_ARRVAL_P(params), key, strlen(key));
            if (val) {
                zend_string *sv = zval_get_string(val);
                smart_str_append(&url, sv);
                zend_string_release(sv);
            }
        } else {
            smart_str_appendc(&url, *p++);
        }

        smart_str_0(&url);
        RETURN_STR(url.s);
    }
}

PHP_METHOD(Router, print) {
    router_object_t *r = Z_ROUTER_P(getThis());

    for (route_entry_t *e = r->routes; e; e = e->next) {
        LOG_INFO("path = %s is middleware %d", e->path, e->middlewares ? 1 : 0);
    }
}

static zend_object *mw_next_new(zend_class_entry *ce) {
    mw_next_object_t *obj = ecalloc(1, sizeof(mw_next_object_t) + zend_object_properties_size(ce));
    zend_object_std_init(&obj->std, ce);
    obj->std.handlers = &lux_mw_next_handlers;
    return &obj->std;
}

static zval make_next(mw_chain_t *chain) {
    zval zv;
    object_init_ex(&zv, lux_mw_next_ce);

    mw_next_object_t *obj = Z_MW_NEXT_OBJ_P(&zv);
    obj->chain            = chain;
    return zv;
}

static void mw_invoke_next(mw_chain_t *chain) {
    if (!chain->current) {
        if (!chain->route_cb) {
            return;
        }
        zval params[3];
        int  pc = 2;
        ZVAL_COPY_VALUE(&params[0], chain->request);
        ZVAL_COPY_VALUE(&params[1], chain->response);

        zval retval;
        php_cb_call(chain->route_cb, pc, params, &retval);
        zval_ptr_dtor(&retval);

        return;
    }

    zval next_zv = make_next(chain);
    zval params[3];
    ZVAL_COPY_VALUE(&params[0], chain->request);
    ZVAL_COPY_VALUE(&params[1], chain->response);
    ZVAL_COPY_VALUE(&params[2], &next_zv);
    zval retval;
    php_cb_call(chain->current->callback, 3, params, &retval);
    zval_ptr_dtor(&next_zv);
    zval_ptr_dtor(&retval);
}

void middleware_run(route_mw_entry_t *middlewares, zval *request, zval *response, php_cb_t *route_cb) {
    mw_chain_t chain = {0};
    chain.current    = middlewares;
    chain.request    = request;
    chain.response   = response;
    chain.route_cb   = route_cb;
    mw_invoke_next(&chain);
}

PHP_METHOD(MwNextHandler, __invoke) {
    mw_next_object_t *obj   = Z_MW_NEXT_OBJ_P(getThis());
    mw_chain_t       *chain = obj->chain;
    if (chain->current) {
        chain->current = chain->current->next;
    }
    mw_invoke_next(chain);
}

ZEND_BEGIN_ARG_INFO_EX(next_invoke, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry mw_next_methods[] = {
    PHP_ME(MwNextHandler, __invoke, next_invoke, ZEND_ACC_PUBLIC)
        PHP_FE_END};

ZEND_BEGIN_ARG_INFO_EX(rt_route, 0, 0, 2)
ZEND_ARG_INFO(0, path)
ZEND_ARG_INFO(0, handler)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(rt_group, 0, 0, 2)
ZEND_ARG_INFO(0, perfix)
ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(rt_ws, 0, 0, 2)
ZEND_ARG_INFO(0, path)
ZEND_ARG_INFO(0, cfg)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(rt_cb, 0, 0, 1)
ZEND_ARG_INFO(0, cb)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(rt_str, 0, 0, 1)
ZEND_ARG_INFO(0, str)
ZEND_END_ARG_INFO()

ZEND_BEGIN_ARG_INFO_EX(rt_void, 0, 0, 0)
ZEND_END_ARG_INFO()

static const zend_function_entry router_methods[] = {
    PHP_ME(Router, get, rt_route, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, post, rt_route, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, put, rt_route, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, delete, rt_route, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, patch, rt_route, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, group, rt_route, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, ws, rt_ws, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, notFound, rt_cb, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, name, rt_str, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, middleware, rt_cb, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, url, rt_str, ZEND_ACC_PUBLIC)
    //
    PHP_ME(Router, print, rt_void, ZEND_ACC_PUBLIC)
    //
    PHP_FE_END
    //
};

LUX_MINIT_FUNCTION(Router) {
    zend_class_entry ce;
    INIT_CLASS_ENTRY(ce, "Lux\\Router", router_methods);
    lux_router_ce                = zend_register_internal_class(&ce);
    lux_router_ce->create_object = router_class_new;
    memcpy(&lux_router_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_router_handlers.offset   = XtOffsetOf(router_object_t, std);
    lux_router_handlers.free_obj = router_class_free_obj;

    INIT_CLASS_ENTRY(ce, "Nexo\\NextHandler", mw_next_methods);
    lux_mw_next_ce                = zend_register_internal_class(&ce);
    lux_mw_next_ce->create_object = mw_next_new;
    memcpy(&lux_mw_next_handlers, zend_get_std_object_handlers(), sizeof(zend_object_handlers));
    lux_mw_next_handlers.offset = XtOffsetOf(mw_next_object_t, std);

    return SUCCESS;
}