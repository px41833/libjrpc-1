#!/bin/sh

#export STAGING_DIR=/home/alaveiw/projects/dj202_openwrt/staging_dir
#export PATH=/home/alaveiw/projects/dj202_openwrt/staging_dir/toolchain-mipsel_24kec+dsp_gcc-4.8-linaro_musl-1.1.11/bin:$PATH

make && make install

#cp install/bin/* /tftpboot
