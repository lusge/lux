#ifndef LUX_ROUTE_H
#define LUX_ROUTE_H

#include "php_lux.h"
#include "util/php_cb.h"
#include "zend_types.h"

static inline unsigned int method_to_id(const char *m) {
    if (strcasecmp(m, "GET") == 0)
        return 1;
    if (strcasecmp(m, "POST") == 0)
        return 3;
    if (strcasecmp(m, "PUT") == 0)
        return 4;
    if (strcasecmp(m, "DELETE") == 0)
        return 0;
    if (strcasecmp(m, "PATCH") == 0)
        return 8;
    return 1;
}

typedef struct _route_mw_entry {
    php_cb_t               *callback;
    struct _route_mw_entry *next;
} route_mw_entry_t;

typedef struct _route_entry {
    unsigned int         method;
    char                *path;
    int                  is_controller;
    char                *controller_class;
    char                *controller_method;
    php_cb_t            *callback;
    route_mw_entry_t    *middlewares;
    char                *name;
    struct _route_entry *next;
} route_entry_t;

typedef struct _route_group {
    char                *prefix;
    struct _route_group *next;
} route_group_t;

typedef struct _ws_entry {
    char             *path;
    php_cb_t         *on_open;
    php_cb_t         *on_message;
    php_cb_t         *on_close;
    struct _ws_entry *next;
} ws_entry_t;

typedef struct {
    route_entry_t *routes;
    route_entry_t *routes_tail;
    ws_entry_t    *ws_routes;
    route_group_t *group_stack;
    php_cb_t      *not_found_cb;
    zend_object    std;
} router_object_t;

extern zend_class_entry *lux_router_ce;

// 匹配地址
route_entry_t *router_match(zval *router_zv, const char *method, const char *path, zval *params_out);

php_cb_t *router_get_not_fount_cb(zval *router_zv);

ws_entry_t *router_ws_match(zval *router_zv, const char *path);

#define Z_ROUTER_P(zv) ((router_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(router_object_t, std)))

/* log 将老的中间件执行方法移至此处。
 * 删除老版本，减少调用次数
 */
typedef struct {
    route_mw_entry_t *current;
    zval             *request;
    zval             *response;
    php_cb_t         *route_cb;
} mw_chain_t;
//
typedef struct {
    mw_chain_t *chain;
    zend_object std;
} mw_next_object_t;

#define Z_MW_NEXT_OBJ_P(zv) ((mw_next_object_t *)((char *)Z_OBJ_P(zv) - XtOffsetOf(mw_next_object_t, std)));

// 执行在路由函数之前
void middleware_run(route_mw_entry_t *middlewares, zval *request, zval *response, php_cb_t *route_cb);

LUX_MINIT_FUNCTION(Router);

#endif //