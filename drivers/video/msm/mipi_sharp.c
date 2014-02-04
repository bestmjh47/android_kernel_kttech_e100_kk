/* Copyright (c) 2010-2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 *
 */

#include "msm_fb.h"
#include "mipi_dsi.h"
#include "mipi_sharp.h"
	// KT TECH : Set Backlight
#include "tps61161_bl.h"

#ifdef CONFIG_SPIDER_SADR
#include <linux/sadr.h>

int lcd_id;
int bl_id;
#endif

static struct msm_panel_common_pdata *mipi_sharp_pdata;

static struct dsi_buf sharp_tx_buf;
static struct dsi_buf sharp_rx_buf;

/* select Power on sequence */
/* if power on sequence is not select , original sequence is performed */
//#define POWER_ON_Sequence_1
//#define POWER_ON_Sequence_GAMMA_2_2
//#define POWER_ON_Sequence_GAMMA_2_4
#define POWER_ON_Sequence_GAMMA_2_6
//#define POWER_ON_Sequence_GAMMA_2_8
//int init_cmd; // flag to distinguish init sequence from slee out sequence

/* POWER ON Sequence */
/* 1. PASSWD1 -> PASSWD2 -> Source CTL -> Power control 1st -> Power control 2nd -> PositiveGamma -> NegativeGamma 
      -> DisplayCTL -> Amp Type -> Sleep Out(WAIT 120ms) -> Display On */
/* 2. MADCTL(WAIT 16ms) -> Amp Type -> Sleep Out(WAIT 120ms) -> Display On */
/* MADCTL */
static char MADCTL[2] = {0x36, 0x40}; /* DTYPE_DCS_WRITE1 */ /* org */

#if defined(POWER_ON_Sequence_1)
/* PASSWD1 */
static char PASSWD1[3] = {0xF0, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* PASSWD2 */
static char PASSWD2[3] = {0xF1, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* Source CTL */
static char SourceCTL[4] = {0xF2, 0x03, 0x35, 0x81}; /* DTYPE_DCS_LWRITE */
/* Power control 1st */
static char PowerControl1st[13] = {0xF4, 0x0A, 0x0B, 0x3E, 0x3E, 0x11, 0x39, 0x1C, 0x04, 0xD2, 0x00, 0x33, 0x33}; /* DTYPE_DCS_LWRITE */
/* Power control 2nd */
/*
PANELCTL3 (F8H)

SHP_VBP[7:0] : VS + VBP = 2 + 2 = 4 = 0x04
SHP_VFP[7:0] : VFP = 2 = 0x02
SHP_HBP[7:0] : HS + HBP = 4 + 58 = 62 = 0x3E
SHP_HFP[7:0] : HFP = 48 = 0x30
*/
static char PowerControl2nd[11] = {0xF8, 0x25, 0x35, 0x35, 0x97, 0x35, 0x10, 0x04, 0x02, 0x3E, 0x30}; /* DTYPE_DCS_LWRITE */



/* Positive Gamma */
static char PositiveGamma[64] = {0xFA, 0x38, 0x3f, 0x24, 0x21, 0x31, 0x31, 0x29, 0x2c, 0x2d, 0x28,
											0x26, 0x26, 0x2a, 0x2d, 0x30, 0x33, 0x36, 0x3a, 0x3a, 0x38,
											0x32, 0x38, 0x3f, 0x21, 0x1e, 0x2e, 0x2f, 0x27, 0x2a, 0x2c,
											0x28, 0x25, 0x25, 0x27, 0x2b, 0x2d, 0x2f, 0x32, 0x35, 0x34, 
											0x31, 0x16, 0x38, 0x3f, 0x1f, 0x1b, 0x2d, 0x2e, 0x27, 0x2b, 
											0x2e, 0x27, 0x2b, 0x28, 0x2a, 0x2c, 0x30, 0x33, 0x35, 0x38, 
											0x3a, 0x37, 0x1b}; /* DTYPE_DCS_LWRITE */
/* Negative Gamma */
static char NegativeGamma[64] = {0xFB, 0x38, 0x3f, 0x24, 0x21, 0x31, 0x31, 0x29, 0x2c, 0x2d, 0x28,
											0x26, 0x26, 0x2a, 0x2d, 0x30, 0x33, 0x36, 0x3a, 0x3a, 0x38,
											0x32, 0x38, 0x3f, 0x21, 0x1e, 0x2e, 0x2f, 0x27, 0x2a, 0x2c,
											0x28, 0x25, 0x25, 0x27, 0x2b, 0x2d, 0x2f, 0x32, 0x35, 0x34,
											0x31, 0x16, 0x38, 0x3f, 0x1f, 0x1b, 0x2d, 0x2e, 0x27, 0x2b,
											0x2e, 0x27, 0x2b, 0x28, 0x2a, 0x2c, 0x30, 0x33, 0x35, 0x38,
											0x3a, 0x37, 0x1b}; /* DTYPE_DCS_LWRITE */
/* Display CTL */
//static char DisplayCTL[2] = {0xEF, 0x20}; /* DTYPE_DCS_WRITE1 */ //org
static char DisplayCTL[2] = {0xEF, 0x30}; /* DTYPE_DCS_WRITE1 */ //modified
#elif defined(POWER_ON_Sequence_GAMMA_2_2)
/* Power ON */
/* Amp Type */
static char AmpType1st[3] = {0xFC, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char AmpType2nd[4] = {0xF5, 0x5A, 0x55, 0x38}; /* DTYPE_DCS_LWRITE */
/* Sleep Out */
static char SleepOut[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
/* PASSWD1 */
static char PASSWD1[3] = {0xF0, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* PASSWD2 */
static char PASSWD2[3] = {0xF1, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* Power control 1st */
static char PowerControl1st[13] = {0xF4, 0x0A, 0x0B, 0x15, 0x1D, 0x11, 0x3B, 0x1C, 0x0C, 0xA2, 0x00, 0x33, 0x33}; /* DTYPE_DCS_LWRITE */
//static char PowerControl1st[13] = {0xF4, 0x0A, 0x0B, 0x3E, 0x3E, 0x11, 0x39, 0x1C, 0x04, 0xD2, 0x00, 0x33, 0x33}; /* DTYPE_DCS_LWRITE */
/* Power control 2nd */
/*
PANELCTL3 (F8H)

SHP_VBP[7:0] : VS + VBP = 2 + 2 = 4 = 0x04
SHP_VFP[7:0] : VFP = 2 = 0x02
SHP_HBP[7:0] : HS + HBP = 4 + 58 = 62 = 0x3E
SHP_HFP[7:0] : HFP = 48 = 0x30
*/
//static char PowerControl2nd[11] = {0xF8, 0x25, 0x35, 0x35, 0x97, 0x35, 0x10, 0x04, 0x02, 0x3E, 0x30}; /* DTYPE_DCS_LWRITE */
static char PowerControl2nd[11] = {0xF8, 0x51, 0x35, 0x35, 0x96, 0x35, 0x10, 0x04, 0x02, 0x0C, 0x10}; /* DTYPE_DCS_LWRITE */
/* Positive Gamma */
static char PositiveGamma[64] = {0xFA, 0x00, 0x19, 0x19, 0x0F, 0x1F, 0x1E, 0x18, 0x17, 0x18, 0x15,
											0x14, 0x14, 0x14, 0x17, 0x18, 0x1B, 0x1E, 0x23, 0x28, 0x2F,
											0x23, 0x1B, 0x0A, 0x0A, 0x00, 0x06, 0x04, 0x00, 0x00, 0x04,
											0x03, 0x07, 0x0D, 0x0F, 0x12, 0x13, 0x16, 0x1B, 0x1F, 0x25, 
											0x2C, 0x21, 0x2A, 0x0A, 0x0C, 0x03, 0x09, 0x08, 0x02, 0x02, 
											0x05, 0x01, 0x03, 0x07, 0x0A, 0x0C, 0x0E, 0x10, 0x15, 0x19, 
											0x21, 0x29, 0x21}; /* DTYPE_DCS_LWRITE */
/* Negative Gamma */
static char NegativeGamma[64] = {0xFB, 0x00, 0x19, 0x19, 0x0F, 0x1F, 0x1E, 0x18, 0x17, 0x18, 0x15,
											0x14, 0x14, 0x14, 0x17, 0x18, 0x1B, 0x1E, 0x23, 0x28, 0x2F,
											0x23, 0x1B, 0x0A, 0x0A, 0x00, 0x06, 0x04, 0x00, 0x00, 0x04,
											0x03, 0x07, 0x0D, 0x0F, 0x12, 0x13, 0x16, 0x1B, 0x1F, 0x25,
											0x2C, 0x21, 0x2A, 0x0A, 0x0C, 0x03, 0x09, 0x08, 0x02, 0x02,
											0x05, 0x01, 0x03, 0x07, 0x0A, 0x0C, 0x0E, 0x10, 0x15, 0x19,
											0x21, 0x29, 0x21}; /* DTYPE_DCS_LWRITE */
/* Display On */
static char DisplayOn[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */

/* POWER OFF */
/* Display Off */
static char DisplayOff[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */

/* SLEEP IN */
static char SleepIn[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */

#elif defined(POWER_ON_Sequence_GAMMA_2_4)
/* Power ON */
/* Amp Type */
static char AmpType1st[3] = {0xFC, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char AmpType2nd[4] = {0xF5, 0x5A, 0x55, 0x38}; /* DTYPE_DCS_LWRITE */
/* Sleep Out */
static char SleepOut[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
/* PASSWD1 */
static char PASSWD1[3] = {0xF0, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* PASSWD2 */
static char PASSWD2[3] = {0xF1, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* Power control 1st */
static char PowerControl1st[13] = {0xF4, 0x0A, 0x0B, 0x2D, 0x25, 0x21, 0x39, 0x1C, 0x0C, 0xA4, 0x00, 0x33, 0x33}; /* DTYPE_DCS_LWRITE */
/* Power control 2nd */
/*
PANELCTL3 (F8H)

SHP_VBP[7:0] : VS + VBP = 2 + 2 = 4 = 0x04
SHP_VFP[7:0] : VFP = 2 = 0x02
SHP_HBP[7:0] : HS + HBP = 4 + 58 = 62 = 0x3E
SHP_HFP[7:0] : HFP = 48 = 0x30
*/
//static char PowerControl2nd[11] = {0xF8, 0x25, 0x35, 0x35, 0x97, 0x35, 0x10, 0x04, 0x02, 0x3E, 0x30}; /* DTYPE_DCS_LWRITE */
static char PowerControl2nd[11] = {0xF8, 0x51, 0x35, 0x35, 0x96, 0x35, 0x10, 0x04, 0x02, 0x0C, 0x10}; /* DTYPE_DCS_LWRITE */
/* Positive Gamma */
static char PositiveGamma[64] = {0xFA, 0x13, 0x20, 0x17, 0x0D, 0x2E, 0x2D, 0x1D, 0x25, 0x26, 0x1C,
											0x16, 0x1D, 0x21, 0x23, 0x25, 0x26, 0x2B, 0x30, 0x35, 0x3A,
											0x28, 0x24, 0x19, 0x0F, 0x0A, 0x15, 0x27, 0x12, 0x13, 0x1C,
											0x18, 0x1B, 0x1D, 0x21, 0x24, 0x26, 0x29, 0x2A, 0x32, 0x35, 
											0x3B, 0x27, 0x28, 0x1F, 0x10, 0x0B, 0x18, 0x19, 0x14, 0x1F, 
											0x1E, 0x19, 0x19, 0x1B, 0x19, 0x21, 0x23, 0x24, 0x29, 0x2E, 
											0x34, 0x39, 0x27}; /* DTYPE_DCS_LWRITE */
/* Negative Gamma */
static char NegativeGamma[64] = {0xFB, 0x13, 0x20, 0x17, 0x0D, 0x2E, 0x2D, 0x1D, 0x25, 0x26, 0x1C,
											0x16, 0x1D, 0x21, 0x23, 0x25, 0x26, 0x2B, 0x30, 0x35, 0x3A,
											0x28, 0x24, 0x19, 0x0F, 0x0A, 0x15, 0x27, 0x12, 0x13, 0x1C,
											0x18, 0x1B, 0x1D, 0x21, 0x24, 0x26, 0x29, 0x2A, 0x32, 0x35,
											0x3B, 0x27, 0x28, 0x1F, 0x10, 0x0B, 0x18, 0x19, 0x14, 0x1F,
											0x1E, 0x19, 0x19, 0x1B, 0x19, 0x21, 0x23, 0x24, 0x29, 0x2E,
											0x34, 0x39, 0x27}; /* DTYPE_DCS_LWRITE */
/* Display On */
static char DisplayOn[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */

/* POWER OFF */
/* Display Off */
static char DisplayOff[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */

/* SLEEP IN */
static char SleepIn[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */

#elif defined(POWER_ON_Sequence_GAMMA_2_6)
/* Power ON */
/* Amp Type */
static char AmpType1st[3] = {0xFC, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char AmpType2nd[4] = {0xF5, 0x5A, 0x55, 0x38}; /* DTYPE_DCS_LWRITE */
/* Sleep Out */
static char SleepOut[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
/* PASSWD1 */
static char PASSWD1[3] = {0xF0, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* PASSWD2 */
static char PASSWD2[3] = {0xF1, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* Power control 1st */
static char PowerControl1st[13] = {0xF4, 0x0A, 0x0B, 0x2D, 0x25, 0x21, 0x39, 0x1C, 0x0C, 0xA4, 0x00, 0x33, 0x33}; /* DTYPE_DCS_LWRITE */
/* Power control 2nd */
/*
PANELCTL3 (F8H)

SHP_VBP[7:0] : VS + VBP = 2 + 2 = 4 = 0x04
SHP_VFP[7:0] : VFP = 2 = 0x02
SHP_HBP[7:0] : HS + HBP = 4 + 58 = 62 = 0x3E
SHP_HFP[7:0] : HFP = 48 = 0x30
*/
//static char PowerControl2nd[11] = {0xF8, 0x25, 0x35, 0x35, 0x97, 0x35, 0x10, 0x04, 0x02, 0x3E, 0x30}; /* DTYPE_DCS_LWRITE */
static char PowerControl2nd[11] = {0xF8, 0x51, 0x35, 0x35, 0x96, 0x35, 0x10, 0x04, 0x02, 0x0C, 0x10}; /* DTYPE_DCS_LWRITE */
/* Positive Gamma */
static char PositiveGamma[64] = {0xFA, 0x13, 0x20, 0x17, 0x0D, 0x2F, 0x2E, 0x20, 0x29, 0x2A, 0x21,
											0x1B, 0x22, 0x26, 0x27, 0x28, 0x27, 0x2E, 0x33, 0x37, 0x3B,
											0x28, 0x24, 0x19, 0x0F, 0x0A, 0x17, 0x28, 0x15, 0x17, 0x20,
											0x1D, 0x20, 0x22, 0x26, 0x28, 0x27, 0x2A, 0x2D, 0x35, 0x38, 
											0x3C, 0x27, 0x28, 0x1F, 0x10, 0x0B, 0x1A, 0x1A, 0x17, 0x23, 
											0x22, 0x1E, 0x1E, 0x20, 0x1E, 0x25, 0x26, 0x25, 0x2C, 0x31, 
											0x37, 0x3A, 0x27}; /* DTYPE_DCS_LWRITE */
/* Negative Gamma */
static char NegativeGamma[64] = {0xFB, 0x13, 0x20, 0x17, 0x0D, 0x2F, 0x2E, 0x20, 0x29, 0x2A, 0x21,
											0x1B, 0x22, 0x26, 0x27, 0x28, 0x27, 0x2E, 0x33, 0x37, 0x3B,
											0x28, 0x24, 0x19, 0x0F, 0x0A, 0x17, 0x28, 0x15, 0x17, 0x20,
											0x1D, 0x20, 0x22, 0x26, 0x28, 0x27, 0x2A, 0x2D, 0x35, 0x38,
											0x3C, 0x27, 0x28, 0x1F, 0x10, 0x0B, 0x1A, 0x1A, 0x17, 0x23,
											0x22, 0x1E, 0x1E, 0x20, 0x1E, 0x25, 0x26, 0x25, 0x2C, 0x31,
											0x37, 0x3A, 0x27}; /* DTYPE_DCS_LWRITE */
/* Display On */
static char DisplayOn[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */

/* POWER OFF */
/* Display Off */
static char DisplayOff[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */

/* SLEEP IN */
static char SleepIn[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */

#elif defined(POWER_ON_Sequence_GAMMA_2_8)
/* Power ON */
/* Amp Type */
static char AmpType1st[3] = {0xFC, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char AmpType2nd[4] = {0xF5, 0x5A, 0x55, 0x38}; /* DTYPE_DCS_LWRITE */
/* Sleep Out */
static char SleepOut[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
/* PASSWD1 */
static char PASSWD1[3] = {0xF0, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* PASSWD2 */
static char PASSWD2[3] = {0xF1, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
/* Power control 1st */
static char PowerControl1st[13] = {0xF4, 0x0A, 0x0B, 0x2D, 0x25, 0x21, 0x39, 0x1C, 0x0C, 0xA4, 0x00, 0x33, 0x33}; /* DTYPE_DCS_LWRITE */
/* Power control 2nd */
/*
PANELCTL3 (F8H)

SHP_VBP[7:0] : VS + VBP = 2 + 2 = 4 = 0x04
SHP_VFP[7:0] : VFP = 2 = 0x02
SHP_HBP[7:0] : HS + HBP = 4 + 58 = 62 = 0x3E
SHP_HFP[7:0] : HFP = 48 = 0x30
*/
//static char PowerControl2nd[11] = {0xF8, 0x25, 0x35, 0x35, 0x97, 0x35, 0x10, 0x04, 0x02, 0x3E, 0x30}; /* DTYPE_DCS_LWRITE */
static char PowerControl2nd[11] = {0xF8, 0x51, 0x35, 0x35, 0x96, 0x35, 0x10, 0x04, 0x02, 0x0C, 0x10}; /* DTYPE_DCS_LWRITE */
/* Positive Gamma */
static char PositiveGamma[64] = {0xFA, 0x14, 0x21, 0x0D, 0x01, 0x09, 0x09, 0x02, 0x05, 0x0E, 0x12,
											0x1E, 0x25, 0x26, 0x29, 0x29, 0x2B, 0x30, 0x34, 0x37, 0x3C,
											0x28, 0x23, 0x1B, 0x0C, 0x00, 0x07, 0x07, 0x00, 0x03, 0x0C,
											0x11, 0x1F, 0x27, 0x28, 0x2C, 0x2B, 0x30, 0x30, 0x37, 0x38, 
											0x3D, 0x28, 0x24, 0x22, 0x0D, 0x01, 0x09, 0x09, 0x02, 0x05, 
											0x0E, 0x12, 0x1E, 0x25, 0x26, 0x29, 0x29, 0x2B, 0x30, 0x34, 
											0x37, 0x3C, 0x28}; /* DTYPE_DCS_LWRITE */
/* Negative Gamma */
static char NegativeGamma[64] = {0xFB, 0x14, 0x21, 0x0D, 0x01, 0x09, 0x09, 0x02, 0x05, 0x0E, 0x12,
											0x1E, 0x25, 0x26, 0x29, 0x29, 0x2B, 0x30, 0x34, 0x37, 0x3C,
											0x28, 0x23, 0x1B, 0x0C, 0x00, 0x07, 0x07, 0x00, 0x03, 0x0C,
											0x11, 0x1F, 0x27, 0x28, 0x2C, 0x2B, 0x30, 0x30, 0x37, 0x38,
											0x3D, 0x28, 0x24, 0x22, 0x0D, 0x01, 0x09, 0x09, 0x02, 0x05,
											0x0E, 0x12, 0x1E, 0x25, 0x26, 0x29, 0x29, 0x2B, 0x30, 0x34,
											0x37, 0x3C, 0x28}; /* DTYPE_DCS_LWRITE */
/* Display On */
static char DisplayOn[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */

/* POWER OFF */
/* Display Off */
static char DisplayOff[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */

/* SLEEP IN */
static char SleepIn[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */
#else /* Original */
/* Amp Type */
static char AmpType1st[3] = {0xFC, 0x5A, 0x5A}; /* DTYPE_DCS_LWRITE */
static char AmpType2nd[4] = {0xF5, 0x5A, 0x55, 0x38}; /* DTYPE_DCS_LWRITE */
/* Sleep Out */
static char SleepOut[2] = {0x11, 0x00}; /* DTYPE_DCS_WRITE */
/* Display On */
static char DisplayOn[2] = {0x29, 0x00}; /* DTYPE_DCS_WRITE */

/* POWER OFF */
/* Display Off */
static char DisplayOff[2] = {0x28, 0x00}; /* DTYPE_DCS_WRITE */
/* SLEEP IN */
static char SleepIn[2] = {0x10, 0x00}; /* DTYPE_DCS_WRITE */

/* Sleep IN */
/* Display Off */
/* SLEEP IN */

/* Sleep OUT */
/* SLEEP Out */
/* Display On */
#endif

#if defined(POWER_ON_Sequence_1)
/* 1. MADCTL(WAIT 16ms) -> PASSWD1 -> PASSWD2 -> Source CTL -> Power control 1st -> Power control 2nd -> PositiveGamma -> NegativeGamma 
      -> DisplayCTL -> Amp Type -> Sleep Out(WAIT 120ms) -> Display On */
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 16,
		sizeof(MADCTL), MADCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(SourceCTL), SourceCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(DisplayCTL), DisplayCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType1st), AmpType1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType2nd), AmpType2nd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
#elif defined(POWER_ON_Sequence_GAMMA_2_2)
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
//static struct dsi_cmd_desc sharp_init_cmds[] = {
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 16,
		sizeof(MADCTL), MADCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType1st), AmpType1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType2nd), AmpType2nd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
/*
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
*/
#elif defined(POWER_ON_Sequence_GAMMA_2_4)
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
//static struct dsi_cmd_desc sharp_init_cmds[] = {
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 16,
		sizeof(MADCTL), MADCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType1st), AmpType1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType2nd), AmpType2nd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
/*
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
*/
#elif defined(POWER_ON_Sequence_GAMMA_2_6)
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
//static struct dsi_cmd_desc sharp_init_cmds[] = {
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 16,
		sizeof(MADCTL), MADCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType1st), AmpType1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType2nd), AmpType2nd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
/*
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
*/
#elif defined(POWER_ON_Sequence_GAMMA_2_8)
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
//static struct dsi_cmd_desc sharp_init_cmds[] = {
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 16,
		sizeof(MADCTL), MADCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType1st), AmpType1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType2nd), AmpType2nd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
/* Original 'sharp_video_on_cmds' of Sharp shows display with flip_LR */
/*
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},	
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD1), PASSWD1},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PASSWD2), PASSWD2},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl1st), PowerControl1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PowerControl2nd), PowerControl2nd},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(PositiveGamma), PositiveGamma},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 16,
		sizeof(NegativeGamma), NegativeGamma},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
*/
#else
/* 2. MADCTL(WAIT 16ms) -> Amp Type -> Sleep Out(WAIT 120ms) -> Display On */
static struct dsi_cmd_desc sharp_video_on_cmds[] = {
	{DTYPE_DCS_WRITE1, 1, 0, 0, 16,
		sizeof(MADCTL), MADCTL},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType1st), AmpType1st},
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(AmpType2nd), AmpType2nd},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepOut), SleepOut},
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn}
};
#endif

static struct dsi_cmd_desc sharp_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOff), DisplayOff},
	{DTYPE_DCS_WRITE, 1, 0, 0, 120,
		sizeof(SleepIn), SleepIn}
};

static char manufacture_id[2] = {0x04, 0x00}; /* DTYPE_DCS_READ */

static struct dsi_cmd_desc sharp_manufacture_id_cmd = {
	DTYPE_DCS_READ, 1, 0, 1, 5, sizeof(manufacture_id), manufacture_id};

static uint32 mipi_sharp_manufacture_id(struct msm_fb_data_type *mfd)
{
	struct dsi_buf *rp, *tp;
	struct dsi_cmd_desc *cmd;
	uint32 *lp;
	printk("[HJM] %s\n", __func__);

	tp = &sharp_tx_buf;
	rp = &sharp_rx_buf;
	mipi_dsi_buf_init(rp);
	mipi_dsi_buf_init(tp);

	cmd = &sharp_manufacture_id_cmd;
	mipi_dsi_cmds_rx(mfd, tp, rp, cmd, 3);
	lp = (uint32 *)rp->data;
	pr_info("%s: manufacture_id=%x", __func__, *lp);
	return *lp;
}

static int mipi_sharp_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
	printk("[HJM] %s\n", __func__);
#ifdef CONFIG_SPIDER_SADR
    sadr_device_on(lcd_id);
#endif

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi  = &mfd->panel_info.mipi;

//	mutex_lock(&mfd->dma->ov_mutex); khkim, lcd bringup
	
	/* Begin - jaemoon.hwang@kttech.co.kr */
	/* 'sharp_video_on_cmds' shows display with flip_LR */
#if 0
	if (!init_cmd) {
			printk("[HJM] %s, if (!init_cmd) {, sharp_init_cmds\n", __func__);
			mipi_dsi_cmds_tx(mfd, &sharp_tx_buf, sharp_init_cmds,
				ARRAY_SIZE(sharp_init_cmds));
			init_cmd = 1;
	}
#endif
	/* End - jaemoon.hwang@kttech.co.kr */
	
	if (mipi->mode == DSI_VIDEO_MODE) {
		mipi_dsi_cmds_tx(mfd, &sharp_tx_buf, sharp_video_on_cmds,
			ARRAY_SIZE(sharp_video_on_cmds));
	} else {	/* not yet implemented */
		mipi_dsi_cmds_tx(mfd, &sharp_tx_buf, sharp_video_on_cmds,
			ARRAY_SIZE(sharp_video_on_cmds));

		mipi_dsi_cmd_bta_sw_trigger(); /* clean up ack_err_status */

		mipi_sharp_manufacture_id(mfd);
	}
//	mutex_unlock(&mfd->dma->ov_mutex); khkim, lcd bringup

	return 0;
}

static int mipi_sharp_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	mfd = platform_get_drvdata(pdev);
	printk("### [HJM] %s, mfd=%0x \n", __func__, (unsigned int)mfd);
#ifdef CONFIG_SPIDER_SADR
    sadr_device_off(lcd_id);
#endif

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	printk("	### before off send cmd.....\n");
//	mutex_lock(&mfd->dma->ov_mutex);
	printk("		### after mutex.....\n");
	mipi_dsi_cmds_tx(mfd, &sharp_tx_buf, sharp_display_off_cmds,
			ARRAY_SIZE(sharp_display_off_cmds));
//	mutex_unlock(&mfd->dma->ov_mutex);

	return 0;
}

static int __devinit mipi_sharp_lcd_probe(struct platform_device *pdev)
{

	printk("### %s, pdev-id=%d\n", __func__, pdev->id);

	if (pdev->id == 0) {
		mipi_sharp_pdata = pdev->dev.platform_data;
		return 0;
	}
	printk("[HJM] %s\n", __func__);
	msm_fb_add_device(pdev);

#ifdef CONFIG_KTTECH_TPS61161_BL
  if ( get_kttech_hw_version() >= ES2_HW_VER )
    tps61161_set_bl_native( 120 );
  else
    tps61161_set_bl_native( 17 );
#endif


#if defined(POWER_ON_Sequence_1)
	printk("[HJM] %s, POWER_ON_Sequence_1\n", __func__);
#elif defined(POWER_ON_Sequence_GAMMA_2_2)
	printk("[HJM] %s, POWER_ON_Sequence_GAMMA_2_2\n", __func__);
#elif defined(POWER_ON_Sequence_GAMMA_2_4)
	printk("[HJM] %s, POWER_ON_Sequence_GAMMA_2_4\n", __func__);
#elif defined(POWER_ON_Sequence_GAMMA_2_6)
	printk("[HJM] %s, POWER_ON_Sequence_GAMMA_2_6\n", __func__);
#elif defined(POWER_ON_Sequence_GAMMA_2_8)
	printk("[HJM] %s, POWER_ON_Sequence_GAMMA_2_8\n", __func__);
#else
	printk("[HJM] %s, Original Sequence without gamma value set by software \n", __func__);
#endif

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_sharp_lcd_probe,
	.driver = {
		.name   = "mipi_sharp",
	},
};

#ifdef CONFIG_SPIDER_SADR
/*
 * LCD
 */
static void sadr_lcd_on(uint32 call_by_sa)
{
	printk("SADR LCD on [%d]\n", call_by_sa);
}

static void sadr_lcd_off(uint32 call_by_sa)
{
	printk("SADR LCD off [%d]\n", call_by_sa);
}

static sadr_control_dev_type sharp_lcd = {
    .name = "sadr_lcd",
    .dev_type = SADR_DEVICE_LCD,
    .device_on = sadr_lcd_on,
    .device_off = sadr_lcd_off,
    .device_set_data = NULL,
    .device_connection = NULL,
    .connect_pri = 1,
    .disconnect_pri = 2,
};

/*
 * Back Light
 */
void sadr_tps61161_set_bl(struct msm_fb_data_type *mfd)
{
	sadr_device_set_data(bl_id, mfd->bl_level);
}

static void sadr_set_bl(uint32 data, uint32 call_by_sa)
{
#ifdef CONFIG_KTTECH_TPS61161_BL
	tps61161_set_bl_native(data);
#endif
}

static sadr_control_dev_type sharp_bl = {
	.name = "sadr_bl",
	.dev_type = SADR_DEVICE_BACKLIGHT,
	.device_set_data = sadr_set_bl,
	.device_on = NULL,
	.device_off = NULL,
	.device_connection = NULL,
	/* Begin - jaemoon.hwang@kttech.co.kr */
	/* max level of backlight level is 29 */
#if 1
	.max_level = 29,
#else
	.max_level = 30,
#endif
	/* End - jaemoon.hwang@kttech.co.kr */
	.connect_pri = 2,
	.disconnect_pri = 1,
};

static struct msm_fb_panel_data sharp_panel_data = {
	.on     = mipi_sharp_lcd_on,
	.off        = mipi_sharp_lcd_off,
	.set_backlight  = sadr_tps61161_set_bl,
};
#else
	// KT TECH : Set Backlight
static struct msm_fb_panel_data sharp_panel_data = {
	.on		= mipi_sharp_lcd_on,
	.off		= mipi_sharp_lcd_off,
#ifdef CONFIG_KTTECH_TPS61161_BL	
	.set_backlight	= tps61161_set_bl,
#endif
};
#endif

static int ch_used[3];

int mipi_sharp_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	printk("### [HJM] %s\n", __func__);
	
	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_sharp", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	sharp_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &sharp_panel_data,
		sizeof(sharp_panel_data));
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_add_data failed!\n", __func__);
		goto err_device_put;
	}

	ret = platform_device_add(pdev);
	if (ret) {
		printk(KERN_ERR
		  "%s: platform_device_register failed!\n", __func__);
		goto err_device_put;
	}

#ifdef CONFIG_SPIDER_SADR
	bl_id = sadr_reg_device(&sharp_bl);
	lcd_id = sadr_reg_device(&sharp_lcd);
#endif

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int __init mipi_sharp_lcd_init(void)
{
	printk("[HJM] %s\n", __func__);
	mipi_dsi_buf_alloc(&sharp_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&sharp_rx_buf, DSI_BUF_SIZE);
	//init_cmd = 0;	// flag to distinguish init sequence from slee out sequence

	return platform_driver_register(&this_driver);
}

module_init(mipi_sharp_lcd_init);
