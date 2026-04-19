/* lux extension for PHP */
#include "http.h"
#include "http_request.h"
#include "http_response.h"
#include "http_websockets.h"

#include "app.h"
#include "controller.h"
#include "ctrl_dispatcher.h"
#include "router.h"
#include "view.h"

#include "php_lux.h"
#include <stdio.h>

/* {{{ PHP_RINIT_FUNCTION */
PHP_RINIT_FUNCTION(lux) {
#if defined(ZTS) && defined(COMPILE_DL_LUX)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif

    return SUCCESS;
}
/* }}} */

/* {{{ PHP_MINFO_FUNCTION */
PHP_MINFO_FUNCTION(lux) {
    php_info_print_table_start();
    php_info_print_table_row(2, "lux support", "enabled");
    php_info_print_table_end();
}
/* }}} */

PHP_MINIT_FUNCTION(lux) {
    LUX_STARTUP(HttpRequest);
    LUX_STARTUP(HttpResponse);
    LUX_STARTUP(HttpWebsockets);
    LUX_STARTUP(Dispatcher);
    LUX_STARTUP(App);
    LUX_STARTUP(Router);
    LUX_STARTUP(Controller);
    LUX_STARTUP(App);
    LUX_STARTUP(View);
    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(lux) { return SUCCESS; }

/* {{{ lux_module_entry */
zend_module_entry lux_module_entry = {
    STANDARD_MODULE_HEADER,
    "lux",              /* Extension name */
    NULL,               /* zend_function_entry */
    PHP_MINIT(lux),     /* PHP_MINIT - Module initialization */
    PHP_MSHUTDOWN(lux), /* PHP_MSHUTDOWN - Module shutdown */
    PHP_RINIT(lux),     /* PHP_RINIT - Request initialization */
    NULL,               /* PHP_RSHUTDOWN - Request shutdown */
    PHP_MINFO(lux),     /* PHP_MINFO - Module info */
    PHP_LUX_VERSION,    /* Version */
    STANDARD_MODULE_PROPERTIES};
/* }}} */

#ifdef COMPILE_DL_LUX
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(lux)
#endif
