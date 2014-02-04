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
#include "mipi_lg_l4500t.h"
// KT TECH : Set Backlight
#include "tps61161_bl.h"

static struct msm_panel_common_pdata *mipi_lg_l4500t_pdata;

static struct dsi_buf lg_l4500t_tx_buf;
static struct dsi_buf lg_l4500t_rx_buf;

//#define KTTECH_MIPI_CMDLIST_PUT

// RESET : ( DTYPE_DCS_WRITE )
//static char Reset[2] = {0x01, 0x00}; 

// SLEEP OUT : ( DTYPE_DCS_WRITE )
static char SleepOut[2] = {0x11, 0x00};

// SLEEP IN  : ( DTYPE_DCS_WRITE )
static char SleepIn[2] = {0x10, 0x00};

// Display On : ( DTYPE_DCS_WRITE )
static char DisplayOn[2] = {0x29, 0x00};

// Display Off : ( DTYPE_DCS_WRITE )
static char DisplayOff[2] = {0x28, 0x00};

// Stand by On : ( DTYPE_DCS_WRITE )
//static char StandByOn[2] = {0x10, 0x00};



// IDs : ( DTYPE_DCS_READ )
//static char manufacture_id[2] = {0x04, 0x00}; 

// MIPI DSI Configuration : ( DTYPE_DCS_LWRITE )
static char MIPI_DSI_Configuration[6] = { 
0xE0 ,
0x43 ,0x40 ,0x80 ,0xFF ,0xFF ,
}; 

// Display Control 1 : ( DTYPE_DCS_LWRITE )
static char Display_Control_1[6] = {
0xB5 ,
0x17 ,0x40 ,0x40 ,0x00 ,0x20 ,
}; 

// Display Control 2 : ( DTYPE_DCS_LWRITE )
static char Display_Control_2[6] = {
0xB6 ,
0x01 ,0x14 ,0x0F ,0x16 ,0x13 ,
}; 

// OTP 2 : ( DTYPE_DCS_WRITE1 )
static char OTP_2[2] = {
0xF9 , 
0x80 ,
}; 

// Internal Oscillator Setting : ( DTYPE_DCS_LWRITE )
static char Internal_Oscillator_Setting[3] = {
0xC0 ,
0x01 ,0x0A ,
}; 

// Power Control 3 : ( DTYPE_DCS_LWRITE )
static char Power_Control_3[10] = {
0xC3 ,
0x00 ,0x09 ,0x10 ,0x12 ,0x00 ,0x66 ,0x00 ,0x32 ,0x00 ,
}; 

// Power Control 4 : ( DTYPE_DCS_LWRITE )
static char Power_Control_4[6] = {
0xC4 ,
0x22 ,0x24 ,0x18 ,0x18 ,0x47 ,
}; 


//#define IMAGE_ENHANCEMENT
//#define IMAGE_ENHANCEMENT_OPTIMUS
//#define IMAGE_ENHANCEMENT_2_2
//#define IMAGE_ENHANCEMENT_2_4
//#define IMAGE_ENHANCEMENT_2_6
//#define IMAGE_ENHANCEMENT_2_8


// Positive Gamma Curve for Red : ( DTYPE_DCS_LWRITE )
#ifdef IMAGE_ENHANCEMENT_2_2
static char Positive_Gamma_Curve_for_Red[10] = {
0xD0 ,
0x00 ,0x10 ,0x66 ,0x20 ,0x14 ,0x06 ,0x31 ,0x31 ,0x01 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_4 )
static char Positive_Gamma_Curve_for_Red[10] = {
0xD0 ,
0x00 ,0x10 ,0x66 ,0x23 ,0x14 ,0x06 ,0x51 ,0x31 ,0x02 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_6 )
static char Positive_Gamma_Curve_for_Red[10] = {
0xD0 ,
0x00 ,0x10 ,0x66 ,0x24 ,0x14 ,0x06 ,0x71 ,0x31 ,0x03 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_8 )
static char Positive_Gamma_Curve_for_Red[10] = {
0xD0 ,
0x00 ,0x10 ,0x66 ,0x27 ,0x14 ,0x06 ,0x71 ,0x31 ,0x04,
}; 
#else
static char Positive_Gamma_Curve_for_Red[10] = {
0xD0 ,
0x00 ,0x10 ,0x66 ,0x23 ,0x14 ,0x06 ,0x51 ,0x31 ,0x02 ,
}; 
#endif

// Negative Gamma Curve for Red : ( DTYPE_DCS_LWRITE )
static char Negative_Gamma_Curve_for_Red[10] = {
0xD1 ,
0x00 ,0x13 ,0x63 ,0x35 ,0x01 ,0x06 ,0x71 ,0x33 ,0x04 ,
}; 

// Positive Gamma Curve for Green : ( DTYPE_DCS_LWRITE )
#ifdef IMAGE_ENHANCEMENT_2_2
static char Positive_Gamma_Curve_for_Green[10] = {
0xD2 ,
0x00 ,0x10 ,0x66 ,0x20 ,0x14 ,0x06 ,0x31 ,0x31 ,0x01 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_4 )
static char Positive_Gamma_Curve_for_Green[10] = {
0xD2 ,
0x00 ,0x10 ,0x66 ,0x23 ,0x14 ,0x06 ,0x51 ,0x31 ,0x02 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_6 )
static char Positive_Gamma_Curve_for_Green[10] = {
0xD2 ,
0x00 ,0x10 ,0x66 ,0x24 ,0x14 ,0x06 ,0x71 ,0x31 ,0x03 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_8 )
static char Positive_Gamma_Curve_for_Green[10] = {
0xD2 ,
0x00 ,0x10 ,0x66 ,0x27 ,0x14 ,0x06 ,0x71 ,0x31 ,0x04,
}; 
#else
static char Positive_Gamma_Curve_for_Green[10] = {
0xD2 ,
0x00 ,0x10 ,0x66 ,0x23 ,0x14 ,0x06 ,0x51 ,0x31 ,0x02 ,
}; 
#endif

// Negative Gamma Curve for Green : ( DTYPE_DCS_LWRITE )
static char Negative_Gamma_Curve_for_Green[10] = {
0xD3 ,
0x00 ,0x13 ,0x63 ,0x35 ,0x01 ,0x06 ,0x71 ,0x33 ,0x04 ,
}; 

// Positive Gamma Curve for Blue : ( DTYPE_DCS_LWRITE )
#ifdef IMAGE_ENHANCEMENT_2_2
static char Positive_Gamma_Curve_for_Blue[10] = {
0xD4 ,
0x00 ,0x10 ,0x66 ,0x20 ,0x14 ,0x06 ,0x31 ,0x31 ,0x01 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_4 )
static char Positive_Gamma_Curve_for_Blue[10] = {
0xD4 ,
0x00 ,0x10 ,0x66 ,0x23 ,0x14 ,0x06 ,0x51 ,0x31 ,0x02 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_6 )
static char Positive_Gamma_Curve_for_Blue[10] = {
0xD4 ,
0x00 ,0x10 ,0x66 ,0x24 ,0x14 ,0x06 ,0x71 ,0x31 ,0x03 ,
}; 
#elif defined( IMAGE_ENHANCEMENT_2_8 )
static char Positive_Gamma_Curve_for_Blue[10] = {
0xD4 ,
0x00 ,0x10 ,0x66 ,0x27 ,0x14 ,0x06 ,0x71 ,0x31 ,0x04,
}; 
#else
static char Positive_Gamma_Curve_for_Blue[10] = {
0xD4 ,
0x00 ,0x10 ,0x66 ,0x23 ,0x14 ,0x06 ,0x51 ,0x31 ,0x02 ,
}; 
#endif

// Negative Gamma Curve for Blue : ( DTYPE_DCS_LWRITE )
static char Negative_Gamma_Curve_for_Blue[10] = {
0xD5 ,
0x00 ,0x13 ,0x63 ,0x35 ,0x01 ,0x06 ,0x71 ,0x33 ,0x04 ,
}; 



#ifdef IMAGE_ENHANCEMENT

static char Image_Enhancement_70[2] = {
0x70 ,
0x0F ,
};

static char Image_Enhancement_71[5] = {
0x71 ,
0x00 , 0x00 , 0x01 , 0x01, 
};

static char Image_Enhancement_72[3] = {
0x72 ,
0x01 , 0x0A, 
};

static char Image_Enhancement_73[4] = {
0x73 ,
0x23, 0x42, 0x00, 
};

static char Image_Enhancement_74[4] = {
0x74 ,
0x04, 0x01, 0x07, 
};

static char Image_Enhancement_75[4] = {
0x75 ,
0x07, 0x00, 0x05, 
};

static char Image_Enhancement_77[9] = {
0x77 ,
0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
};

static char Image_Enhancement_78[9] = {
0x78 ,
0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
};

#ifdef IMAGE_ENHANCEMENT_OPTIMUS
static char Image_Enhancement_79[9] = {
0x79 ,
0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 0x3A, 
};
#else
static char Image_Enhancement_79[9] = {
0x79 ,
0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 0x40, 
};
#endif

static char Image_Enhancement_7A[9] = {
0x7A ,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

static char Image_Enhancement_7B[9] = {
0x7B ,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

static char Image_Enhancement_7C[9] = {
0x7C ,
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
};

#endif

// Power Control 1 : ( DTYPE_DCS_WRITE1 )
static char Power_Control_1[2] = {
0xC1 , 
0x00 ,
}; 

// Power Control 1 0x01: ( DTYPE_DCS_WRITE1 )
static char Power_Control_1_0x01[2] = {
0xC1 , 
0x01 ,
};
// Power Control 1 0x02: ( DTYPE_DCS_WRITE1 )
static char Power_Control_1_0x02[2] = {
0xC1 , 
0x02 ,
};


// Power Control 2 0x00 : ( DTYPE_DCS_WRITE1 )
static char Power_Control_2[2] = {
0xC2 , 
0x00 ,
}; 

// Power Control 2 0x02: ( DTYPE_DCS_WRITE1 )
static char Power_Control_2_0x02[2] = {
0xC2 , 
0x02 ,
}; 

// Power Control 2 0x06 : ( DTYPE_DCS_WRITE1 )
static char Power_Control_2_0x06[2] = {
0xC2 , 
0x06 ,
}; 

// Power Control 2 0x4E : ( DTYPE_DCS_WRITE1 )
static char Power_Control_2_0x4E[2] = {
0xC2 , 
0x4E ,
}; 


static struct dsi_cmd_desc lg_l4500t_video_on_cmds[] = {
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(MIPI_DSI_Configuration), MIPI_DSI_Configuration},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Display_Control_1), Display_Control_1},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Display_Control_2), Display_Control_2},		

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(OTP_2), OTP_2},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Internal_Oscillator_Setting), Internal_Oscillator_Setting},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Power_Control_3), Power_Control_3},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Power_Control_4), Power_Control_4},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Positive_Gamma_Curve_for_Red), Positive_Gamma_Curve_for_Red},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Negative_Gamma_Curve_for_Red), Negative_Gamma_Curve_for_Red},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Positive_Gamma_Curve_for_Green), Positive_Gamma_Curve_for_Green},		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Negative_Gamma_Curve_for_Green), Negative_Gamma_Curve_for_Green},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Positive_Gamma_Curve_for_Blue), Positive_Gamma_Curve_for_Blue},		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Negative_Gamma_Curve_for_Blue), Negative_Gamma_Curve_for_Blue},		
		
#ifdef IMAGE_ENHANCEMENT

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Image_Enhancement_70), Image_Enhancement_70},

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_71), Image_Enhancement_71},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_72), Image_Enhancement_72},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_73), Image_Enhancement_73},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_74), Image_Enhancement_74},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_75), Image_Enhancement_75},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_77), Image_Enhancement_77},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_78), Image_Enhancement_78},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_79), Image_Enhancement_79},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_7A), Image_Enhancement_7A},		
		
	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_7B), Image_Enhancement_7B},		

	{DTYPE_DCS_LWRITE, 1, 0, 0, 0,
		sizeof(Image_Enhancement_7C), Image_Enhancement_7C},		
#endif

	{DTYPE_DCS_WRITE1, 1, 0, 0, 100,
		sizeof(Power_Control_1), Power_Control_1},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 40,
		sizeof(Power_Control_2_0x02), Power_Control_2_0x02},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 40,
		sizeof(Power_Control_2_0x06), Power_Control_2_0x06},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 40,
		sizeof(Power_Control_2_0x4E), Power_Control_2_0x4E},

	{DTYPE_DCS_WRITE, 1, 0, 0, 10,
		sizeof(SleepOut), SleepOut},

	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(DisplayOn), DisplayOn},

};


static struct dsi_cmd_desc lg_l4500t_display_off_cmds[] = {
	{DTYPE_DCS_WRITE, 1, 0, 0, 20,
		sizeof(DisplayOff), DisplayOff},

	{DTYPE_DCS_WRITE, 1, 0, 0, 0,
		sizeof(SleepIn), SleepIn},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 20,
		sizeof(Power_Control_2), Power_Control_2},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Power_Control_1_0x02), Power_Control_1_0x02},

	{DTYPE_DCS_WRITE1, 1, 0, 0, 0,
		sizeof(Power_Control_1_0x01), Power_Control_1_0x01},		
};

static int mipi_lg_l4500t_lcd_on(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
	struct mipi_panel_info *mipi;
#ifdef KTTECH_MIPI_CMDLIST_PUT
	struct dcs_cmd_req cmdreq;
#endif

	printk("### %s\n", __func__);

	mfd = platform_get_drvdata(pdev);
	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	mipi  = &mfd->panel_info.mipi;

	if (mipi->mode == DSI_VIDEO_MODE) 
	{
		// KTTech modify : mipi_dsi_cmds_tx -> mipi_dsi_cmdlist_put
#ifdef KTTECH_MIPI_CMDLIST_PUT
		cmdreq.cmds = lg_l4500t_video_on_cmds;
		cmdreq.cmds_cnt = ARRAY_SIZE(lg_l4500t_video_on_cmds);
		cmdreq.flags = CMD_REQ_COMMIT;
		cmdreq.rlen = 0;
		cmdreq.cb = NULL;
		mipi_dsi_cmdlist_put(&cmdreq);
#else
		mipi_dsi_cmds_tx(&lg_l4500t_tx_buf, lg_l4500t_video_on_cmds, ARRAY_SIZE(lg_l4500t_video_on_cmds));	
#endif
		printk("%s, MIPI MOD = DSI_VIDEO_MODE \n", __func__);
	} else {	/* not yet implemented */
		printk("%s, MIPI MOD = DSI_CMD_MODE Error!!! \n", __func__);
	}

	return 0;
}

static int mipi_lg_l4500t_lcd_off(struct platform_device *pdev)
{
	struct msm_fb_data_type *mfd;
#ifdef KTTECH_MIPI_CMDLIST_PUT
	struct dcs_cmd_req cmdreq;
#endif

	mfd = platform_get_drvdata(pdev);
	printk("### %s, mfd=%0x \n", __func__, (unsigned int)mfd);

	if (!mfd)
		return -ENODEV;
	if (mfd->key != MFD_KEY)
		return -EINVAL;

	// KTTech modify : mipi_dsi_cmds_tx -> mipi_dsi_cmdlist_put
#ifdef KTTECH_MIPI_CMDLIST_PUT
	cmdreq.cmds = lg_l4500t_display_off_cmds;
	cmdreq.cmds_cnt = ARRAY_SIZE(lg_l4500t_display_off_cmds);
	cmdreq.flags = CMD_REQ_COMMIT;
	cmdreq.rlen = 0;
	cmdreq.cb = NULL;
	mipi_dsi_cmdlist_put(&cmdreq);
#else
	mipi_dsi_cmds_tx(&lg_l4500t_tx_buf, lg_l4500t_display_off_cmds,
			ARRAY_SIZE(lg_l4500t_display_off_cmds));
#endif

	return 0;
}

static int __devinit mipi_lg_l4500t_lcd_probe(struct platform_device *pdev)
{

	printk("### %s, pdev-id=%d\n", __func__, pdev->id);

	if (pdev->id == 0) {
		mipi_lg_l4500t_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

#ifdef CONFIG_KTTECH_TPS61161_BL
	/* KT Tech 2012.12.07
	** 1. Continuous splash screen Enabled
	** 2. TAKE Logo Disabled
	*/
	//tps61161_set_bl_native(0);
#endif

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_lg_l4500t_lcd_probe,
	.driver = {
		.name   = "mipi_lg_l4500t"
	},
};

// KT TECH : Set Backlight
static struct msm_fb_panel_data lg_l4500t_panel_data = {
	.on		= mipi_lg_l4500t_lcd_on,
	.off		= mipi_lg_l4500t_lcd_off,
#ifdef CONFIG_KTTECH_TPS61161_BL	
	.set_backlight	= tps61161_set_bl,
#endif
};

static int ch_used[3];

int mipi_lg_l4500t_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_lg_l4500t", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	printk("### %s, platform_device_alloc \n", __func__);
	lg_l4500t_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &lg_l4500t_panel_data,
		sizeof(lg_l4500t_panel_data));
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

	return 0;

err_device_put:
	platform_device_put(pdev);
	return ret;
}

static int __init mipi_lg_l4500t_lcd_init(void)
{
	printk("%s \n", __func__);
	mipi_dsi_buf_alloc(&lg_l4500t_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&lg_l4500t_rx_buf, DSI_BUF_SIZE);
	//init_cmd = 0;	// flag to distinguish init sequence from slee out sequence

	return platform_driver_register(&this_driver);
}

module_init(mipi_lg_l4500t_lcd_init);
