# AC_PREREQ(2.61)
AC_INIT([TrainC], [0.5.1], [rybach@cs.rwth-aachen.de])
AM_INIT_AUTOMAKE([-Wall -Werror foreign])
AC_REVISION([$Rev$])


AC_CONFIG_SRCDIR([src/context_builder.h])
AM_CONFIG_HEADER([src/config.h])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_CONFIG_MACRO_DIR([m4])
AC_LANG([C++])

# Checks for programs.
AC_PROG_CC
AC_PROG_CXX
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_RANLIB


# Checks for libraries.
AC_ARG_WITH([openfst-include-path],
  [AS_HELP_STRING([--with-openfst-include-path],
    [location of the OpenFst headers, defaults to /usr/local/include])],
  [OPENFST_CFLAGS="-I$withval"],
  [OPENFST_CFLAGS='-I/usr/local/include'])
AC_SUBST([OPENFST_CFLAGS])

AC_ARG_WITH([openfst-lib-path],
  [AS_HELP_STRING([--with-openfst-lib-path], [location of the OpenFst libraries])],
  [OPENFST_LIBS="-L$withval -lfst -ldl"],
  [OPENFST_LIBS='-lfst -ldl'])
AC_SUBST([OPENFST_LIBS])

CPPFLAGS="$CPPFLAGS $OPENFST_CFLAGS"
LDFLAGS="$LDFLAGS $OPENFST_LIBS"

AC_CHECK_HEADER([fst/fstlib.h], [], AC_MSG_ERROR("OpenFst not found"))

AC_ARG_WITH([google-perftools],
  [AS_HELP_STRING([--with-google-perftools], [location of the Google Performance Tools])],
  [PERFTOOLS_LIBS="-L${withval}/lib -L${withval}/lib64 -ltcmalloc"],[])
AC_SUBST([PERFTOOLS_LIBS])

LDFLAGS="$LDFLAGS $PERFTOOLS_LIBS"
AC_CHECK_LIB( [tcmalloc], [tc_malloc], [], [])

AC_ARG_ENABLE([threads],
  [AS_HELP_STRING([--enable-threads], [add multi-threading support])],
  [enable_threads="$enableval"])
AM_CONDITIONAL([HAVE_THREADS], [test "x$enable_threads" = xyes])

AS_IF([test "x$enable_threads" = xyes],
  [AC_CHECK_HEADERS([pthread.h], ,
    AC_MSG_ERROR([pthread.h required]))
   AC_CHECK_LIB([pthread], [pthread_join], , 
    AC_MSG_ERROR([libpthread required]))
   AC_DEFINE([HAVE_THREADS], [], [multi-thread support])
  ])


AC_ARG_ENABLE([debug],
  [AS_HELP_STRING([--enable-debug], [add debug information])],
  [ADD_DEBUG="$enableval"],
  [ADD_DEBUG="no"])
AC_SUBST([ADD_DEBUG])  
  
if test x$ADD_DEBUG = xyes; then
	CPPFLAGS="$CPPFLAGS -DDEBUG"
fi
AM_CONDITIONAL([DEBUG], [test x$ADD_DEBUG = xyes])

AC_ARG_ENABLE([tests],
  [AS_HELP_STRING([--enable-tests], [build unit tests])],
  [WITH_TESTS="$enableval"],
  [WITH_TESTS="no"])
AC_SUBST([WITH_TESTS])
AM_CONDITIONAL([WITH_TESTS], [test x$WITH_TESTS = xyes])

AC_CHECK_HEADER([cppunit/Test.h], [HAVE_CPPUNIT=yes], [HAVE_CPPUNIT=no])
AC_SUBST([HAVE_CPPUNIT])

if test x$WITH_TESTS = xyes; then
  if test x$HAVE_CPPUNIT != xyes; then
    AC_MSG_ERROR("CppUnit is required to build unit tests")
  fi
fi

AC_CHECK_HEADERS([execinfo.h])

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.
# AC_CHECK_TYPES([std::tr1::hash<long long unsigned>], [], [],
#               [[#include <tr1/unordered_map>]])

AC_CHECK_HEADER([ext/hash_map], [],
              AC_MSG_ERROR([ext/hash_map not found]),[])
AC_CHECK_HEADER([ext/numeric], [],
              AC_MSG_ERROR([ext/numeric not found]),[])

# AC_HEADER_STDBOOL
# AC_C_CONST
# AC_C_INLINE
# AC_TYPE_SIZE_T

# Checks for library functions.
# AC_HEADER_STDC

AC_OUTPUT
