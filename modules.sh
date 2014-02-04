#############################
# Take LTE SH Kernel Source #
# by SungHun.Ra             #
# http://theteamdev.com     #
#############################
rm -rf ../Modules/*.ko
find -name '*.ko' -exec cp -av {} ../Modules \;
        for i in ../Modules/*; do /home/moon/toolchain/arm-eabi-4.6/bin/arm-eabi-strip --strip-unneeded $i;done;\
