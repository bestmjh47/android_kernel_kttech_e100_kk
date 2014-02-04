/*
 * aat2862_bl_ctrl.c - Backlignt control chip
 *
 * Copyright (C) 2010 KT Tech
 * 
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/miscdevice.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/gpio.h>
#include <linux/input.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <mach/pmic.h>
#include <linux/mfd/pm8xxx/pm8921.h>

#include "msm_fb.h"
#include "tps61161_easyscale_bl.h"

#ifdef CONFIG_KTTECH_TPS61161_PWM_BL
#include "tps61161_pwm_bl.h"
#endif

void tps61161_set_bl_native(__u32 lcd_backlight_level)
{
#ifdef CONFIG_KTTECH_TPS61161_PWM_BL
  if ( 1 ) //get_kttech_hw_version() >= ES2_HW_VER )
  {
    tps61161_set_bl_native_pwm( lcd_backlight_level );
  }
  else
#endif
  {
    tps61161_set_bl_native_easyscale( lcd_backlight_level );
  }

/*
	int i, bit_coding;
	unsigned long flags;
	
	printk(KERN_DEBUG "###### tps61161_set_bl_native : %d \n", lcd_backlight_level);

	mutex_lock(&tps6116_bl);
	
	if(lcd_backlight_level == 0) {
		spin_lock_irqsave(&tps61161_spin_lock, flags);
#ifdef 	__KTTECH_BL_USE_PM_GPIO__	
		gpio_set_value_cansleep(BL_ON_GPIO, 0);
#else
		gpio_set_value(BL_ON_GPIO, 0);
#endif
		enter_easyscale_set = 0;
		spin_unlock_irqrestore(&tps61161_spin_lock, flags);
		msleep(3);	
	}
	else {
		spin_lock_irqsave(&tps61161_spin_lock, flags);
		if (enter_easyscale_set == 0) {
			EASYSCALE_MODE_SET();
    		spin_unlock_irqrestore(&tps61161_spin_lock, flags);
          	msleep(1);
    		spin_lock_irqsave(&tps61161_spin_lock, flags);
		}

		T_START();

		tps61161_set_device_id();

		T_EOS();	

		T_START();

		LOW_BIT();  // 0 Request for Ack
		LOW_BIT();  // 0 Address bit 1
		LOW_BIT();  // 0 Address bit 0

		for(i = BL_DIMING_DATA_CNT; i >= 0; i--)
		{
			bit_coding = ((lcd_backlight_level >> i) & 0x1); // (lcd_backlight_level & (2 ^ i));

			if(bit_coding) {
				HIGH_BIT(); // Data High
			} else {
				LOW_BIT();  // Data Low
			}
		}

		T_EOS();
		STATIC_HIGH();
		enter_easyscale_set = 1;
		udelay(500);//udelay(100); 
		spin_unlock_irqrestore(&tps61161_spin_lock, flags);
	}
    mutex_unlock(&tps6116_bl);
*/    
}

EXPORT_SYMBOL(tps61161_set_bl_native);

void tps61161_set_bl(struct msm_fb_data_type *mfd)
{
//	tps61161_set_bl_native(mfd->bl_level);
#ifdef CONFIG_KTTECH_TPS61161_PWM_BL
  if ( 1 ) //get_kttech_hw_version() >= ES2_HW_VER )
  {
    tps61161_set_bl_native_pwm( mfd->bl_level );
  }
  else
#endif
  {
    tps61161_set_bl_native_easyscale( mfd->bl_level );
  }
}

EXPORT_SYMBOL(tps61161_set_bl);

static int __init tps61161_init(void)
{
#ifdef CONFIG_KTTECH_TPS61161_PWM_BL
  if ( 1 ) //get_kttech_hw_version() >= ES2_HW_VER )
  {
    return tps61161_init_pwm();
  }
  else
#endif
  {
    return tps61161_init_easyscale();
  }

/*
	int rc = 0;

#ifdef __KTTECH_BL_USE_PM_GPIO__
	struct pm_gpio blonio_param = {
		.direction = PM_GPIO_DIR_OUT,
		.output_buffer = PM_GPIO_OUT_BUF_CMOS,
		.output_value = 0,
		.pull = PM_GPIO_PULL_NO,
		.vin_sel = 2,
		.out_strength = PM_GPIO_STRENGTH_HIGH,
		.function = PM_GPIO_FUNC_NORMAL,
		.inv_int_pol = 0,
		.disable_pin = 0,
	};
#endif

	printk("### %s, gpio request... bl gpio=%d\n", __func__, BL_ON_GPIO);

	rc += gpio_request(BL_ON_GPIO, "tps61161_init");
	if (rc) {
		pr_err("### BL_ON_GPIO failed (3), rc=%d\n", rc);
		return -EINVAL;
	}
	
#ifdef __KTTECH_BL_USE_PM_GPIO__
	rc = pm8xxx_gpio_config(BL_ON_GPIO, &blonio_param);
	pr_err("### tps pm8xx gpio config. ret=%d \n", rc);
#else
	rc += gpio_tlmm_config(GPIO_CFG(BL_ON_GPIO, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);	
	rc += gpio_direction_output(BL_ON_GPIO, 1);	
#endif	
	mutex_init(&tps6116_bl);
	spin_lock_init(&tps61161_spin_lock);

	if(!rc) 
		printk("LCD Backlight Controller Initialized.\n");
		
	return rc;
*/

}

static void __exit tps61161_exit(void)
{
//	gpio_free(BL_ON_GPIO);
#ifdef CONFIG_KTTECH_TPS61161_PWM_BL
  if ( 1 ) //get_kttech_hw_version() >= ES2_HW_VER )
  {
    return tps61161_exit_pwm();
  }
  else
#endif
  {
    return tps61161_exit_easyscale();
  }
}

module_init(tps61161_init);
module_exit(tps61161_exit);

