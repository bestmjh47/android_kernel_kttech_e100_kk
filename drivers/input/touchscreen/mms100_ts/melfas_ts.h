/*
 * include/linux/melfas_ts.h - platform data structure for MCS Series sensor
 *
 * Copyright (C) 2010 Melfas, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_MELFAS_TS_H
#define _LINUX_MELFAS_TS_H

#define MELFAS_TS_NAME "melfas-ts"

#define USING_GPIO_FOR_VDD 1
//#define USING_LVS6 1

struct melfas_tsi_platform_data {
	int x_size;
	int y_size;
	int  version;
	int (*power)(int on); // modified
};

#define TOUCH_TYPE_NONE			0
#define TOUCH_TYPE_SCREEN		1
#define TOUCH_TYPE_KEY			2

#define TS_CMD_SET_FRAMERATE    0x10
#define TS_CMD_SET_MERGE	    0x11
#define TS_CMD_SET_EDIT			0x12
#define TS_CMD_SET_ESD			0x13
#define TS_CMD_SET_CORRPOS_X    0x40
#define TS_CMD_SET_CORRPOS_Y    0x80

#define TS_VNDR_CMDID           0xB0
#define TS_VNDR_CMD_RESULT      0xBF

#define DEBUG_LOW_POSITION       (0x1 << 0)

#define ENABLE_ESD_DETECT_LIMIT_TIME	1000
#define ESD_RETRY_COUNTER_LIMIT			3
#endif /* _LINUX_MELFAS_TS_H */
