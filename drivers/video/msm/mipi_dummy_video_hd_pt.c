/* Copyright (c) 2009-2010, Code Aurora Forum. All rights reserved.
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
#include "mipi_dummy.h"

static struct msm_panel_info pinfo;

#define DSI_BIT_CLK_480MHZ
//#define DSI_BIT_CLK_500MHZ
 
static struct mipi_dsi_phy_ctrl dsi_video_mode_phy_db = {
	/* 600*1024, RGB888, 3 Lane 55 fps video mode */
    /* regulator */
	{0x03, 0x0a, 0x04, 0x00, 0x20},

	/* timing */
	{0xac, 0x8b, 0x19, /* panel specific */
	 0x00, /* DSIPHY_TIMING_CTRL_3 = 0 */
	 0x1b, 0x91, 0x1c, 0x8d, 0x1b, 0x03, 0x04}, /* panel specific */

    /* phy ctrl */
	{0x5f, 0x00, 0x00, 0x10},
    /* strength */
	{0xff, 0x00, 0x06, 0x00},

	/* pll control */
	{0x0, /* common 8960 */
	 /* VCO */
	 0x77, 0x1, 0x19, /* panel specific */
	 0x00, 0x50, 0x48, 0x63,
	 0x31, 0x0F, 0x03,/* 4 lane Auto update by dsi-mipi driver */
	 0x00, 0x14, 0x03, 0x00, 0x02, /* common 8960 */
	 0x00, 0x20, 0x00, 0x01 }, /* common 8960 */
};

static int __init mipi_video_dummy_hd_pt_init(void)
{
	int ret;

#if 1//def CONFIG_FB_MSM_MIPI_PANEL_DETECT
	if (msm_fb_detect_client("mipi_video_dummy_hd"))
		return 0;
#endif

	printk("### %s, \n", __func__);


	pinfo.mipi.xres_pad = 0;
	pinfo.mipi.yres_pad = 0;
	
	pinfo.xres = 720;
	pinfo.yres = 1280;
	pinfo.type = MIPI_VIDEO_PANEL;
	pinfo.pdest = DISPLAY_1;
	pinfo.wait_cycle = 0;
	pinfo.bpp = 24;

/*********************************
	LG Recommand value
	1. 
		h bp 83
		  fp 12
		  pw 4
		v bp 32
		  fp 8
		  pw 2
	2.
		h bp 82
		  fp 12
		  pw 4
		v bp 32
		  fp 10
		  pw 2
**********************************/
#if defined(DSI_BIT_CLK_500MHZ)
	pinfo.lcdc.h_back_porch = 83; // 84
	pinfo.lcdc.h_front_porch = 12; 
	pinfo.lcdc.h_pulse_width = 4; 
	pinfo.lcdc.v_back_porch = 32;// 2
	pinfo.lcdc.v_front_porch = 8;// 2
	pinfo.lcdc.v_pulse_width = 2; // 2
#elif defined(DSI_BIT_CLK_480MHZ)
	pinfo.lcdc.h_back_porch = 83; // 84
	pinfo.lcdc.h_front_porch = 12; 
	pinfo.lcdc.h_pulse_width = 4; 
	pinfo.lcdc.v_back_porch = 32;
	pinfo.lcdc.v_front_porch = 8;
	pinfo.lcdc.v_pulse_width = 2;
#else
	pinfo.lcdc.h_back_porch = 83; // 84
	pinfo.lcdc.h_front_porch = 12; 
	pinfo.lcdc.h_pulse_width = 4; 
	pinfo.lcdc.v_back_porch = 32;
	pinfo.lcdc.v_front_porch = 8;
	pinfo.lcdc.v_pulse_width = 2;
#endif

	pinfo.lcdc.border_clr = 0xff0000;	/* blk */
	/* Begin - jaemoon.hwang@kttech.co.kr */
	/* Change underflow color from blue to black */
#if 1
	pinfo.lcdc.underflow_clr = 0;	/* blk  */
#else
	pinfo.lcdc.underflow_clr = 0xff;	/* blue */
#endif
	/* End - jaemoon.hwang@kttech.co.kr */
	pinfo.lcdc.hsync_skew = 0;
	/* Begin - jaemoon.hwang@kttech.co.kr */
	/* bl_max is changed from 28 to 29 */
#if 1
	pinfo.bl_max = 29;
#else
	pinfo.bl_max = 28;
#endif
	/* End - jaemoon.hwang@kttech.co.kr */
	/* Begin - jaemoon.hwang@kttech.co.kr */
	/* bl_min is changed from 1 to 4 */
#if 1
	pinfo.bl_min = 4;
#else
	pinfo.bl_min = 1;
#endif
	/* End - jaemoon.hwang@kttech.co.kr */
	pinfo.fb_num = 2;

#if defined(DSI_BIT_CLK_500MHZ)
	pinfo.clk_rate = 500000000;
#elif defined(DSI_BIT_CLK_480MHZ)
	pinfo.clk_rate = 480000000;
#else		/* 200 mhz */
	pinfo.clk_rate = 200000000;
#endif
	pinfo.mipi.mode = DSI_VIDEO_MODE;
	pinfo.mipi.pulse_mode_hsa_he = TRUE;
	pinfo.mipi.hfp_power_stop = TRUE;
	pinfo.mipi.hbp_power_stop = TRUE;
	pinfo.mipi.hsa_power_stop = TRUE;
	pinfo.mipi.eof_bllp_power_stop = 0; //lcd ���� ���� ���� 
	pinfo.mipi.bllp_power_stop = 0;  /* Needed or else will have blank line at top of display */
	pinfo.mipi.traffic_mode = DSI_BURST_MODE;
	pinfo.mipi.dst_format = DSI_VIDEO_DST_FORMAT_RGB888;
	pinfo.mipi.vc = 0;
	pinfo.mipi.rgb_swap = DSI_RGB_SWAP_RGB;
	pinfo.mipi.data_lane0 = TRUE;
	pinfo.mipi.data_lane1 = TRUE;
	pinfo.mipi.data_lane2 = TRUE;
	pinfo.mipi.data_lane3 = TRUE;
	pinfo.mipi.tx_eot_append = TRUE;

#if defined(DSI_BIT_CLK_480MHZ)
	pinfo.mipi.t_clk_post = 34;
	pinfo.mipi.t_clk_pre = 61;
#elif defined(DSI_BIT_CLK_500MHZ)
	pinfo.mipi.t_clk_post = 34;
	pinfo.mipi.t_clk_pre = 61;
#else
	pinfo.mipi.t_clk_post = 34;
	pinfo.mipi.t_clk_pre = 61;
#endif
	
	pinfo.mipi.stream = 0; /* dma_p */
	pinfo.mipi.mdp_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.dma_trigger = DSI_CMD_TRIGGER_SW;
	pinfo.mipi.frame_rate = 60;
	pinfo.mipi.dsi_phy_db = &dsi_video_mode_phy_db;

	ret = mipi_dummy_device_register(&pinfo, MIPI_DSI_PRIM,
						MIPI_DSI_PANEL_WVGA_PT);
	if (ret)
		pr_err("%s: failed to register device!\n", __func__);

	return ret;
}

module_init(mipi_video_dummy_hd_pt_init);

