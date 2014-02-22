#!/bin/sh
###################################
# Take LTE Ultimate Kernel Source #             
#           bestmjh47             #
###################################
TOOLCHAINPATH=/home/moon/toolchain/linaro-4.8.3/bin
export ARCH=arm
#export CROSS_COMPILE=/home/moon/toolchain/arm-linux-androideabi-4.6/bin/arm-linux-androideabi-
export CROSS_COMPILE=$TOOLCHAINPATH/arm-gnueabi-
#export CROSS_COMPILE=/home/moon/toolchain/android-toolchain-eabi-4.8/bin/arm-eabi-
make bestmjh47_defconfig
echo #############################
echo #       Now Starting...     #
echo #############################
make -j15
echo Compiling Finished!
cp arch/arm/boot/zImage zImage
echo Striping Modules...
rm -rf modules/*
find -name '*.ko' -exec cp -av {} Modules \;
        for i in Modules/*; do $TOOLCHAINPATH/arm-gnueabi-strip --strip-unneeded $i;done;\
mkdir -p Modules/etc/firmware/wlan/prima
cp drivers/staging/prima/firmware_bin/WCNSS_cfg.dat Modules/etc/firmware/wlan/prima
cp drivers/staging/prima/firmware_bin/WCNSS_qcom_cfg.ini Modules/etc/firmware/wlan/prima
cp drivers/staging/prima/firmware_bin/WCNSS_qcom_wlan_nv.bin Modules/etc/firmware/wlan/prima
echo ""
echo Done! zImage is READY!!!
echo Making bootimg for direct flashing...
ramdisk/mkbootimg --cmdline "androidboot.hardware=qcom user_debug=31 msm_rtb.filter=0x3F ehci-hcd.park=3 no_console_suspend=1" --base 0x80200000 --pagesize 2048 --ramdiskaddr 0x82200000 --kernel zImage --ramdisk ramdisk/bestmjh47-ramdisk.gz -o boot-e100.img
echo ""
echo done! 

