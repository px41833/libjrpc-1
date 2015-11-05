EXTRA_DIST =
lib_LTLIBRARIES = libjrpc.la

AM_CPPFLAGS = \
    -include $(top_builddir)/config.h \
    -DSYSCONFDIR=\""$(sysconfdir)"\" \
    -DLIBEXECDIR=\""$(libexecdir)"\" \
    -I${top_srcdir}/src

STAGING_DIR=/home/alaveiw/projects/dj202_openwrt/staging_dir

libc_incdir = ${STAGING_DIR}/target-mipsel_24kec+dsp_musl-1.1.11/usr/include
libc_libdir = ${STAGING_DIR}/target-mipsel_24kec+dsp_musl-1.1.11/usr/lib

AM_CFLAGS = ${my_CFLAGS}

libjrpc_la_SOURCES = \
        jrpc.c ipsc.c

libjrpc_la_LDFLAGS = -no-undefined \
        -version-info $(LIBJRPC_LT_VERSION_INFO)

libjrpc_la_CFLAGS = -I$(libc_incdir)

libjrpc_la_LIBADD = -L$(libc_libdir) \
					  -ljansson
#if OS_WIN32
#libjrpc_la_LIBADD = -lwsock32
#endif
#
#if OS_QNX
#libjrpc_la_LIBADD = -lsocket
#endif

# Header files to install
#libjrpcincludedir = $(includedir)/jrpc

pkginclude_HEADERS = jrpc.h ipsc.h

