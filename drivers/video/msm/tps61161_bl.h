#ifndef TPS61161_BL_CTRL_KTTECH_H
#define TPS61161_BL_CTRL_KTTECH_H

#include "tps61161_pwm_bl.h"
#include "tps61161_easyscale_bl.h"

/*===========================================================================
  FUNCTIONS PROTOTYPES
============================================================================*/
void tps61161_set_bl_native(__u32 lcd_backlight_level);
void tps61161_set_bl(struct msm_fb_data_type *mfd);


#endif /* TPS61161_BL_CTRL_KTTECH_H */

