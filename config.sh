libtoolize
aclocal
autoheader
autoconf
automake
#./configure --prefix=/home/alaveiw/study/autotoolsTEST/libjrpc/lib


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

./configure \
    --host=mipsel-linux \
    --build=i686-pc-linux-gnu \
    --prefix=$INSTALL_DIR \
    --target=mipsel-linux \
    --enable-static
