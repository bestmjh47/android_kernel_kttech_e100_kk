#ifndef TPS61161_BL_EASYSCALE_CTRL_KTTECH_H
#define TPS61161_BL_EASYSCALE_CTRL_KTTECH_H

#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/fb.h>
#include <linux/msm_mdp.h>
#include <linux/init.h>
#include <linux/ioport.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/uaccess.h>
#include <linux/workqueue.h>
#include <linux/string.h>
#include <linux/version.h>
#include <linux/proc_fs.h>
#include <linux/vmalloc.h>
#include <linux/debugfs.h>
#include <linux/console.h>
#include <linux/android_pmem.h>

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/time.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include "linux/proc_fs.h"
#include <mach/hardware.h>
#include <linux/io.h>
#include <linux/fb.h>
#include <asm/system.h>
#include <asm/mach-types.h>
#include <linux/platform_device.h>

#include <linux/board_kttech.h>

/*===========================================================================
  FUNCTIONS PROTOTYPES
============================================================================*/
//void tps61161_set_bl_easyscale(struct msm_fb_data_type *mfd);
void tps61161_set_bl_native_easyscale(int lcd_backlight_level);

int tps61161_init_easyscale(void);
void tps61161_exit_easyscale(void);

#endif /* TPS61161_BL_EASYSCALE_CTRL_KTTECH_H */

