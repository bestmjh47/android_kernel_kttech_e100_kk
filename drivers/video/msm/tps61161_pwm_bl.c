#include "msm_fb.h"

#include <linux/pwm.h>
#include <linux/mfd/pm8xxx/pm8921.h>
#include <linux/board_kttech.h>

#include "tps61161_pwm_bl.h"

static struct pwm_device *bl_lpm;

#ifdef CONFIG_KTTECH_PWM_NANO_SEC_CTRL
extern int pwm_config_nsec_ctrl(struct pwm_device *pwm, int duty_ns, int period_ns);
#endif

void tps61161_set_bl_native_pwm (__u32 lcd_backlight_level)
{
	int ret;

	pr_debug("###### %s : %d \n", __func__, lcd_backlight_level);

	if (bl_lpm) {
#ifdef CONFIG_KTTECH_PWM_NANO_SEC_CTRL	
		ret = pwm_config_nsec_ctrl(bl_lpm, TPS61161_BL_PWM_DUTY_LEVEL *
			lcd_backlight_level , TPS61161_BL_PWM_PERIOD_NSEC);
#else
		ret = pwm_config(bl_lpm, TPS61161_BL_PWM_DUTY_LEVEL *
			lcd_backlight_level , TPS61161_BL_PWM_PERIOD_USEC);
#endif
		if (ret) {
			pr_err("pwm_config on lpm failed %d\n", ret);
			return;
		}
		if (lcd_backlight_level) {
			ret = pwm_enable(bl_lpm);
			if (ret)
				pr_err("pwm enable/disable on lpm failed"
					"for bl %d\n",	lcd_backlight_level );
		} else {
			pwm_disable(bl_lpm);
		}
	}
}

EXPORT_SYMBOL(tps61161_set_bl_native_pwm);


int tps61161_init_pwm( void )
{  
	bl_lpm = pwm_request( TPS61161_PWM_LPM_CHANNEL , "backlight");

	if (bl_lpm == NULL || IS_ERR(bl_lpm)) {
		pr_err("%s pwm_request() failed\n", __func__);
		bl_lpm = NULL;
		return -1;
	}

  return 0;
}
EXPORT_SYMBOL(tps61161_init_pwm);


void tps61161_exit_pwm(void)
{

}

EXPORT_SYMBOL(tps61161_exit_pwm);



