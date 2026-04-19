/* lux extension for PHP */

#ifndef PHP_LUX_H
#define PHP_LUX_H
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include "php.h"

#include "Zend/zend_API.h"
#include "Zend/zend_exceptions.h"
#include "ext/standard/info.h"
#include "zend_interfaces.h"
#include "zend_objects.h"

#include "ext/standard/php_var.h"

#define LUX_STARTUP(module) \
    ZEND_MODULE_STARTUP_N(lux_##module)(INIT_FUNC_ARGS_PASSTHRU)
#define LUX_MINIT_FUNCTION(module) ZEND_MINIT_FUNCTION(lux_##module)

#define LUX_SHUTDOWN(module) \
    ZEND_MODULE_SHUTDOWN_N(lux_##module)(SHUTDOWN_FUNC_ARGS_PASSTHRU)
#define LUX_MSHUTDOWN_FUNCTION(module) ZEND_MSHUTDOWN_FUNCTION(lux_##module)

extern zend_module_entry lux_module_entry;
#define phpext_lux_ptr &lux_module_entry

#define PHP_LUX_VERSION "0.1.0"

#if defined(ZTS) && defined(COMPILE_DL_LUX)
ZEND_TSRMLS_CACHE_EXTERN()
#endif

#endif /* PHP_LUX_H */
