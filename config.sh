#libtoolize
#aclocal
#autoheader
#autoconf
#automake


TOP_DIR=`pwd`
INSTALL_DIR="$TOP_DIR"/install

export CROSS_COMPILE=mipsel-openwrt-linux
export CC=$CROSS_COMPILE-gcc
#export NM=$CROSS_COMPILE-nm
export AR=$CROSS_COMPILE-ar
#export AS=$CROSS_COMPILE-as
export RANLIB=$CROSS_COMPILE-ranlib
#export OBJDUMP=$CROSS_COMPILE-objdump
export STRIP=$CROSS_COMPILE-strip

export STAGING_DIR=/home/gomma/projects/openwrt/staging_dir
export CFLAGS=-I$STAGING_DIR/target-mipsel_24kec+dsp_musl-1.1.11/usr/include
export LDFLAGS=-L$STAGING_DIR/target-mipsel_24kec+dsp_musl-1.1.11/usr/lib -lgansson

./configure \
    --host=mipsel-linux \
    --build=i686-pc-linux-gnu \
    --prefix=$INSTALL_DIR \
    --target=mipsel-linux \
    --enable-static
