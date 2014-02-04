#!/bin/sh
#############################
# Take LTE SH Kernel Source #             
# http://theteamdev.com     #
# & bestmjh47               #
#############################
export ARCH=arm
export CROSS_COMPILE=/home/moon/toolchain/arm-linux-androideabi-4.6/bin/arm-linux-androideabi-
#export CROSS_COMPILE=/home/moon/linaro-4.8.1/bin/arm-cortex_a8-linux-gnueabi-
#export CROSS_COMPILE=/home/moon/toolchain/arm-linux-androideabi-4.7/bin/arm-linux-androideabi-
#export CROSS_COMPILE=/home/moon/toolchain/android-toolchain-eabi-4.8/bin/arm-eabi-
make bestmjh47_defconfig
make -j8
