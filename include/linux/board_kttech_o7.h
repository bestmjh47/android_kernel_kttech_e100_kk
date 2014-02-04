/* kernel/include/linux/board_kttech_o7.h
 * Copyright (C) 2007-2012 KTTech Corporation.
 * Author:  <hyunkuk007@kttech.co.kr>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
*/
#ifndef _BOARD_KTTECH_O7_H_
#define _BOARD_KTTECH_O7_H_

//-----------------------------------------------------------------------------
// 1. GPIO
//-----------------------------------------------------------------------------

// Backlight
#define TPS61161_PWM_LPM_CHANNEL 0                    // case pwm type 
#if defined(CONFIG_KTTECH_TARGET_O7_WS1_KCONFIG)
#define BL_ON_GPIO       (PM8921_GPIO_PM_TO_SYS(24))  // case easyscale type 
#else
#define BL_ON_GPIO       (69)
#endif


#define LCD_RESET_GPIO   (PM8921_GPIO_PM_TO_SYS(43))

// Camera
#define CAM1_RST_N		107
#define CAM2_RST_N		76
#define CAM_5M_EN			58
#define TORCH_ENABLE		3
#define FLASH_ENABLE		63

//TSP
#define TSP_3_3V_EN				51
#define MELFAS_TSP_SDA			16
#define MELFAS_TSP_SCL			17

//-----------------------------------------------------------------------------
// 2. Interrupt
//-----------------------------------------------------------------------------
//TSP
#define MELFAS_TSP_INT			11

//-----------------------------------------------------------------------------
// 3. VREG
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// 4. I2C bus id
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// 5. USB
//-----------------------------------------------------------------------------


//-----------------------------------------------------------------------------
// 6. Others
//-----------------------------------------------------------------------------


#endif /* _BOARD_KTTECH_O7_H_ */
