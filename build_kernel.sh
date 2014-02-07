#!/bin/sh
#############################
# Take LTE SH Kernel Source #             
# http://theteamdev.com     #
# & bestmjh47               #
#############################
TOOLCHAINPATH=/home/moon/toolchain/linaro-4.8.3/bin
export ARCH=arm
#export CROSS_COMPILE=/home/moon/toolchain/arm-linux-androideabi-4.6/bin/arm-linux-androideabi-
export CROSS_COMPILE=$TOOLCHAINPATH/arm-gnueabi-
#export CROSS_COMPILE=/home/moon/toolchain/android-toolchain-eabi-4.8/bin/arm-eabi-
make bestmjh47_defconfig
echo #############################
echo #       Now Starting...     #
echo #############################
make -j12
echo Compiling Finished!
echo Striping Modules...
rm -rf Modules/*
find -name '*.ko' -exec cp -av {} Modules \;
        for i in Modules/*; do $TOOLCHAINPATH/arm-gnueabi-strip --strip-unneeded $i;done;\
echo ""
echo Done!
