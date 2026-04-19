PHP_ARG_ENABLE([lux],
  [whether to enable lux support],
  [AS_HELP_STRING([--enable-lux],
    [Enable lux support])],
  [no])

AS_VAR_IF([PHP_LUX], [no],, [
  
  AC_DEFINE([HAVE_LUX], [1],
    [Define to 1 if the PHP extension 'lux' is available.])

  PHP_ADD_INCLUDE($srcdir/deps/facil_io)
  # PHP_ADD_INCLUDE($srcdir/deps/libev)
  PHP_ADD_INCLUDE($srcdir/src)

  PHP_NEW_EXTENSION([lux],
    m4_normalize([
      deps/facil_io/fio.c \
      deps/facil_io/fio_siphash.c \
      deps/facil_io/fio_tls_missing.c \
      deps/facil_io/fio_tls_openssl.c \
      deps/facil_io/fiobj_ary.c \
      deps/facil_io/fiobj_data.c  \
      deps/facil_io/fiobj_hash.c  \
      deps/facil_io/fiobj_json.c  \
      deps/facil_io/fiobj_mustache.c  \
      deps/facil_io/fiobj_numbers.c \
      deps/facil_io/fiobj_str.c \
      deps/facil_io/http_internal.c \
      deps/facil_io/http1.c \
      deps/facil_io/redis_engine.c  \
      deps/facil_io/fiobject.c \
      deps/facil_io/http.c \
      deps/facil_io/websockets.c  \
      src/lux.c \
      src/http_request.c \
      src/http_response.c  \
      src/http_websockets.c \
      src/ctrl_dispatcher.c \
      src/controller.c  \
      src/view.c  \
      src/app.c  \
      src/router.c  \
      src/util/log.c  \
      src/util/php_cb.c
    ]),
    [$ext_shared],,
    [-DZEND_ENABLE_STATIC_TSRMLS_CACHE=1])
])
