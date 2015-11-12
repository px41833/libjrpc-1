#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.68])
AC_INIT(libjrpc, 1.0.0, alaveiw@raonix.com)
AM_INIT_AUTOMAKE([check-news foreign 1.11 -Wall -Wno-portability silent-rules tar-pax subdir-objects])
AC_CONFIG_SRCDIR([src/ipsc.h])
AC_CONFIG_HEADERS([config.h])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.

# Checks for header files.
AC_CHECK_HEADERS([fcntl.h stdlib.h string.h sys/socket.h syslog.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_INLINE
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T


LIBJRPC_LD_CURRENT=6
LIBJRPC_LD_REVISION=0
LIBJRPC_LD_AGE=1
LIBJRPC_LT_VERSION_INFO=$LIBJRPC_LD_CURRENT:$LIBJRPC_LD_REVISION:$LIBJRPC_LD_AGE
AC_SUBST(LIBJRPC_LT_VERSION_INFO)



LT_INIT([disable-static win32-dll pic-only])

# Checks for library functions.
AC_FUNC_MALLOC
AC_FUNC_REALLOC
AC_CHECK_FUNCS([socket])

AC_CONFIG_FILES([Makefile
                 src/Makefile
				 libjrpc.pc
				 ])
AC_OUTPUT
