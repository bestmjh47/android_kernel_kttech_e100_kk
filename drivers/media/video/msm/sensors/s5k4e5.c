/* Copyright (c) 2011, Code Aurora Forum. All rights reserved.
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
 */

#include <linux/module.h>
#include "msm_sensor.h"
#include "s5k4e5.h"
#define SENSOR_NAME "s5k4e5"
/* Begin - jaemoon.hwang@kttech.co.kr */
/* CONFIG_CAMERA_CALIBRATION_EEPROM */
#if 1
#define SENSOR_EEPROM_NAME "s5k4e5_eeprom"
#endif
/* End - jaemoon.hwang@kttech.co.kr */
#define PLATFORM_DRIVER_NAME "msm_camera_s5k4e5"
#define s5k4e5_obj s5k4e5_##obj

DEFINE_MUTEX(s5k4e5_mut);
static struct msm_sensor_ctrl_t s5k4e5_s_ctrl;
/* Begin - jaemoon.hwang@kttech.co.kr */
/* CONFIG_CAMERA_CALIBRATION_EEPROM */
#if 1
static struct msm_sensor_ctrl_t s5k4e5_eeprom_s_ctrl;
#endif
/* End - jaemoon.hwang@kttech.co.kr */

static struct msm_camera_i2c_reg_conf s5k4e5_start_settings[] = {
	{0x0100, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e5_stop_settings[] = {
	{0x0100, 0x00},
};

static struct msm_camera_i2c_reg_conf s5k4e5_groupon_settings[] = {
	{0x0104, 0x01},
};

static struct msm_camera_i2c_reg_conf s5k4e5_groupoff_settings[] = {
	{0x0104, 0x0},
};

static struct msm_camera_i2c_reg_conf s5k4e5_prev_settings[] = {

	// Integration setting ... 				
	{0x0200, 0x03},		// fine integration time				
	{0x0201, 0x5C},				
	{0x0202, 0x03},		// coarse integration time				
	{0x0203, 0xd0},				
	{0x0204, 0x00},		// Analog Gain				
	{0x0205, 0x20},				
	{0x0340, 0x07},		// Frame Length				
	{0x0341, 0xB4},				
	{0x0342, 0x0A},		//line_length_pclk			
	{0x0343, 0xB2},				
	//Size Setting ...				
	// 2608x1960				
	{0x30A9, 0x03},		//Horizontal Binning Off				
	{0x300E, 0x28},		//Vertical Binning Off				
	{0x302B, 0x01},				
	{0x3029, 0x34},		// DBLR & PLA				
	{0x0380, 0x00},		//x_even_inc 1				
	{0x0381, 0x01},
	{0x0382, 0x00},		//x_odd_inc 1
	{0x0383, 0x01},
	{0x0384, 0x00},		//y_even_inc 1
	{0x0385, 0x01},
	{0x0386, 0x00},		//y_odd_inc 3
	{0x0387, 0x01},
	{0x0344, 0x00},		//x_addr_start
	{0x0345, 0x00},
	{0x0346, 0x00},		//y_addr_start
	{0x0347, 0x00},
	{0x0348, 0x0A},		//x_addr_end
	{0x0349, 0x2F},
	{0x034A, 0x07},		//y_addr_end
	{0x034B, 0xA7},
	{0x034C, 0x0A},		//x_output_size_High
	{0x034D, 0x30},
	{0x034E, 0x07},		//y_output_size_High
	{0x034F, 0xA8},
};

static struct msm_camera_i2c_reg_conf s5k4e5_snap_settings[] = {
	// Integration setting ... 				
	{0x0200, 0x03},		// fine integration time				
	{0x0201, 0x5C},				
	{0x0202, 0x03},		// coarse integration time				
	{0x0203, 0xd0},				
	{0x0204, 0x00},		// Analog Gain				
	{0x0205, 0x20},				
	{0x0340, 0x07},		// Frame Length				
	{0x0341, 0xB4},				
	{0x0342, 0x0A},		//line_length_pclk			
	{0x0343, 0xB2},				
	//Size Setting ...				
	// 2608x1960				
	{0x30A9, 0x03},		//Horizontal Binning Off				
	{0x300E, 0x28},		//Vertical Binning Off				
	{0x302B, 0x01},				
	{0x3029, 0x34},		// DBLR & PLA				
	{0x0380, 0x00},		//x_even_inc 1				
	{0x0381, 0x01},
	{0x0382, 0x00},		//x_odd_inc 1
	{0x0383, 0x01},
	{0x0384, 0x00},		//y_even_inc 1
	{0x0385, 0x01},
	{0x0386, 0x00},		//y_odd_inc 3
	{0x0387, 0x01},
	{0x0344, 0x00},		//x_addr_start
	{0x0345, 0x00},
	{0x0346, 0x00},		//y_addr_start
	{0x0347, 0x00},
	{0x0348, 0x0A},		//x_addr_end
	{0x0349, 0x2F},
	{0x034A, 0x07},		//y_addr_end
	{0x034B, 0xA7},
	{0x034C, 0x0A},		//x_output_size_High
	{0x034D, 0x30},
	{0x034E, 0x07},		//y_output_size_High
	{0x034F, 0xA8},
};

static struct msm_camera_i2c_reg_conf s5k4e5_1080p_settings[] = {
	// Integration setting ... 				
	{0x0200, 0x03},		// fine integration time				
	{0x0201, 0x5C},				
	{0x0202, 0x03},		// coarse integration time				
	{0x0203, 0xd0},				
	{0x0204, 0x00},		// Analog Gain				
	{0x0205, 0x20},				
	{0x0340, 0x07},		// Frame Length				
	{0x0341, 0xB4},				
	{0x0342, 0x0A},		//line_length_pclk
	{0x0343, 0xB2},				
	//Size Setting ...				
	// 2608x1960				
	{0x30A9, 0x03},		//Horizontal Binning Off
	{0x300E, 0x28},		//Vertical Binning Off
	{0x302B, 0x01},				
	{0x3029, 0x34},		// DBLR & PLA				
	{0x0380, 0x00},		//x_even_inc 1				
	{0x0381, 0x01},
	{0x0382, 0x00},		//x_odd_inc 1
	{0x0383, 0x01},
	{0x0384, 0x00},		//y_even_inc 1
	{0x0385, 0x01},
	{0x0386, 0x00},		//y_odd_inc 3
	{0x0387, 0x01},
	{0x0344, 0x00},		//x_addr_start
	{0x0345, 0x00},
	{0x0346, 0x00},		//y_addr_start
	{0x0347, 0x00},
	{0x0348, 0x0A},		//x_addr_end
	{0x0349, 0x2F},
	{0x034A, 0x07},		//y_addr_end
	{0x034B, 0xA7},
	{0x034C, 0x0A},		//x_output_size_High
	{0x034D, 0x30},
	{0x034E, 0x07},		//y_output_size_High
	{0x034F, 0xA8},
};

static struct msm_camera_i2c_reg_conf s5k4e5_recommend_settings[] = {

	// Sensor reset
	{0x0103, 0x1},		// TBD : test				
	
	//+++++++++++++++++++++++++++++++//
	// Analog Setting
	//// CDS timing setting ...
	{0x3000, 0x05},
	{0x3001, 0x03},
	{0x3002, 0x08},
	{0x3003, 0x0A},
	{0x3004, 0x50},
	{0x3005, 0x0E},
	{0x3006, 0x5E},
	{0x3007, 0x00},
	{0x3008, 0x78},
	{0x3009, 0x78},
	{0x300A, 0x50},
	{0x300B, 0x08},
	{0x300C, 0x14},
	{0x300D, 0x00},
	{0x300F, 0x40},
	{0x301B, 0x77},

	// CDS option setting ...
	{0x3010, 0x00},
	{0x3011, 0x3A},
	{0x3012, 0x30},
	{0x3013, 0xA0},
	{0x3014, 0x00},
	{0x3015, 0x00},
	{0x3016, 0x52},
	{0x3017, 0x94},
	{0x3018, 0x70},
	{0x301D, 0xD4},
	{0x3021, 0x02},
	{0x3022, 0x24},
	{0x3024, 0x40},
	{0x3027, 0x08},

	// Pixel option setting ...      
	{0x301C, 0x06},
	{0x30D8, 0x3F},

	//+++++++++++++++++++++++++++++++//
	// ADLC setting ...
	{0x3070, 0x5F},
	{0x3071, 0x00},
	{0x3080, 0x04},
	{0x3081, 0x38},
	{0x302E, 0x0B},

	//+++++++++++++++++++++++++++++++++//
	//Mirror setting
	{0x0101, 0x00}, // kuzuri_jb from O7 main.. O9: 0x03
  
	// MIPI setting				
	{0x30BD, 0x00},		//SEL_CCP[0]				
	{0x3084, 0x15},		//SYNC Mode				
	{0x30BE, 0x1A},		//M_PCLKDIV_AUTO[4], M_DIV_PCLK[3:0]				
	{0x30C1, 0x01},		//pack video enable [0]				
	{0x30EE, 0x02},		//DPHY enable [1]				
	{0x3111, 0x86},		//Embedded data off [5]				
	{0x30E8, 0x0F},		//Continuous mode 			
	{0x30E3, 0x38},		//According to MCLK			
	{0x30E4, 0x40},				
	{0x3113, 0x70},				
	{0x3114, 0x80},				
	{0x3115, 0x7B},				
	{0x3116, 0xC0},				
	{0x30EE, 0x12},				

	// PLL setting ...				
	{0x0305, 0x06},				
	{0x0306, 0x00},
	{0x0307, 0x64},				
	{0x30B5, 0x00},
	{0x30E2, 0x02},		//num lanes[1:0] = 2				
	{0x30F1, 0xD0},		// DPHY Band Control			
	{0x30BC, 0xB0},		// [7]bit : DBLR enable, [6]~[3]bit : DBLR Div			
										// DBLR clock = Pll output/DBLR Div = 66.6Mhz			
	{0x30BF, 0xAB},		//outif_enable[7], data_type[5:0](2Bh = bayer 10bit)				
	{0x30C0, 0x80},		//video_offset[7:4]				
	{0x30C8, 0x0C},		//video_data_length 				
	{0x30C9, 0xBC},				
	{0x3112, 0x00},		//gain option sel off			
	{0x3030, 0x07},		//old shut mode	
	
};

static struct v4l2_subdev_info s5k4e5_subdev_info[] = {
	{
	.code   = V4L2_MBUS_FMT_SBGGR10_1X10,
	.colorspace = V4L2_COLORSPACE_JPEG,
	.fmt    = 1,
	.order    = 0,
	},
	/* more can be supported, to be added later */
};

static struct msm_camera_i2c_conf_array s5k4e5_init_conf[] = {
	{&s5k4e5_recommend_settings[0],
	ARRAY_SIZE(s5k4e5_recommend_settings), 0, MSM_CAMERA_I2C_BYTE_DATA}
};

static struct msm_camera_i2c_conf_array s5k4e5_confs[] = {
	{&s5k4e5_prev_settings[0],
	ARRAY_SIZE(s5k4e5_prev_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k4e5_snap_settings[0],
	ARRAY_SIZE(s5k4e5_snap_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
	{&s5k4e5_1080p_settings[0],
	ARRAY_SIZE(s5k4e5_1080p_settings), 0, MSM_CAMERA_I2C_BYTE_DATA},
};

static struct msm_sensor_output_info_t s5k4e5_dimensions[] = {
	{
		.x_output = 0xA30,
		.y_output = 0x7A8,
		//.line_length_pclk = 0x85c,	//jaemoon.hwang
		.line_length_pclk = 0xAB2,
		//.frame_length_lines = 0x460,	//jaemoon.hwang
		.frame_length_lines = 0x7B4,
		.vt_pixel_clk = 160000000,
		.op_pixel_clk = 160000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0xA30,
		.y_output = 0x7A8,
		//.line_length_pclk = 0x85c,	//jaemoon.hwang
		.line_length_pclk = 0xAB2,
		//.frame_length_lines = 0x460,	//jaemoon.hwang
		.frame_length_lines = 0x7B4,
		.vt_pixel_clk = 160000000,
		.op_pixel_clk = 160000000,
		.binning_factor = 1,
	},
	{
		.x_output = 0xA30,
		.y_output = 0x7A8,
		//.line_length_pclk = 0x85c,	//jaemoon.hwang
		.line_length_pclk = 0xAB2,
		//.frame_length_lines = 0x460,	//jaemoon.hwang
		.frame_length_lines = 0x7B4,
		.vt_pixel_clk = 160000000,
		.op_pixel_clk = 160000000,
		.binning_factor = 1,
	},
};


#if 1
static struct msm_camera_csid_vc_cfg s5k4e5_cid_cfg[] = {
	{0, CSI_RAW10, CSI_DECODE_10BIT},
	{1, CSI_EMBED_DATA, CSI_DECODE_8BIT},
};

static struct msm_camera_csi2_params s5k4e5_csi_params = {
	.csid_params = {
		.lane_assign = 0xe4,
		.lane_cnt = 2,
		.lut_params = {
			.num_cid = 2,
			.vc_cfg = s5k4e5_cid_cfg,
		},
	},
	.csiphy_params = {
		.lane_cnt = 2,
		//.settle_cnt = 0x1B,
		.settle_cnt = 0x14,	//from s5k3h2
	},
};

static struct msm_camera_csi2_params *s5k4e5_csi_params_array[] = {
	&s5k4e5_csi_params,
	&s5k4e5_csi_params,
	&s5k4e5_csi_params,
};

#else
static struct msm_camera_csi_params s5k4e5_csi_params = {
	.data_format = CSI_10BIT,
	.lane_cnt    = 2,
	.lane_assign = 0xe4,
	.dpcm_scheme = 0,
	.settle_cnt  = 24,
};

static struct msm_camera_csi_params *s5k4e5_csi_params_array[] = {
	&s5k4e5_csi_params,
	&s5k4e5_csi_params,
};

#endif

static struct msm_sensor_output_reg_addr_t s5k4e5_reg_addr = {
	.x_output = 0x034C,
	.y_output = 0x034E,
	.line_length_pclk = 0x0342,
	.frame_length_lines = 0x0340,	
};

static struct msm_sensor_id_info_t s5k4e5_id_info = {
	.sensor_id_reg_addr = 0x0000,
	//.sensor_id = 0x4E10,	//jaemoon.hwang
	.sensor_id = 0x4E50,
};

static struct msm_sensor_exp_gain_info_t s5k4e5_exp_gain_info = {
	.coarse_int_time_addr = 0x0202,
	.global_gain_addr = 0x0204,
	//.vert_offset = 3,
	.vert_offset = 8, 		//from s5k3h2  // kuzuri_jb from O9 perf
//	.vert_offset = 12, 		//from s5k3h2  // kuzuri_test_cam
};

static int32_t s5k4e5_write_exp_gain(struct msm_sensor_ctrl_t *s_ctrl,
		uint16_t gain, uint32_t line)
{
	/* Begin - jaemoon.hwang@kttech.co.kr */
	/* remove red line noise in low light environment */
	/* This code comes from msm8260 */
	/* So, needs debuging based on msm8960 */
#if 1
	uint16_t max_legal_gain = 0x0200;
	int32_t rc = 0;
	static uint32_t fl_lines;

  printk("S5k_sensor: write_exp_gain(): Max legal gain= 0x%X at Line:%d\n", gain, __LINE__);
	if (gain > max_legal_gain) {
		gain = max_legal_gain;
	}
	/* Analogue Gain */
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0204, 
		(gain&0xFF00)>>8, MSM_CAMERA_I2C_BYTE_DATA);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0205, 
		gain&0xFF, MSM_CAMERA_I2C_BYTE_DATA);
	//printk("org fl_lines(0x%x), gain(0x%x) @LINE:%d \n", s_ctrl->curr_frame_length_lines, gain, __LINE__);

	if (line > (s_ctrl->curr_frame_length_lines - 4)) {
		fl_lines = line+4;
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0340, 
			(fl_lines&0xFF00)>>8, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0341, 
			fl_lines&0xFF, MSM_CAMERA_I2C_BYTE_DATA);

		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0202, 
			(line&0xFF00)>>8, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0203, 
			line&0xFF, MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
		//printk("fl_lines(0x%x), line(0x%x) @LINE:%d \n", fl_lines, line, __LINE__);
		
	} else if (line < (fl_lines - 4)) {
		fl_lines = line+4;
		if (fl_lines < s_ctrl->curr_frame_length_lines)
			fl_lines = s_ctrl->curr_frame_length_lines;

		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0340, 
			(fl_lines&0xFF00)>>8, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0341, 
			fl_lines&0xFF, MSM_CAMERA_I2C_BYTE_DATA);

		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0202, 
			(line&0xFF00)>>8, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0203, 
			line&0xFF, MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
		//printk("fl_lines(0x%x), line(0x%x) @LINE:%d \n", fl_lines, line, __LINE__);

	} else {
		fl_lines = line+4;
		s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0340, 
			(fl_lines&0xFF00)>>8, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0341, 
			fl_lines&0xFF, MSM_CAMERA_I2C_BYTE_DATA);

		/* Coarse Integration Time */
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0202, 
			(line&0xFF00)>>8, MSM_CAMERA_I2C_BYTE_DATA);
		msm_camera_i2c_write(s_ctrl->sensor_i2c_client, 0x0203, 
			line&0xFF, MSM_CAMERA_I2C_BYTE_DATA);
		s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
		//printk("fl_lines(0x%x), line(0x%x) @LINE:%d \n", fl_lines, line, __LINE__);
	}
	return rc;
#else
	uint32_t fl_lines, offset;
	uint8_t int_time[3];
	fl_lines =
		(s_ctrl->curr_frame_length_lines * s_ctrl->fps_divider) / Q10;
	offset = s_ctrl->sensor_exp_gain_info->vert_offset;
	if (line > (fl_lines - offset))
		fl_lines = line + offset;

	/* Begin - jaemoon.hwang@kttech.co.kr */
	/* temporarily hard-code exposure gain */
	/* This code will be changed when camera tuning is performed */
#if 1
	gain |= 0x20;
#endif
	/* End - jaemoon.hwang@kttech.co.kr */

	s_ctrl->func_tbl->sensor_group_hold_on(s_ctrl);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_output_reg_addr->frame_length_lines, fl_lines,
		MSM_CAMERA_I2C_WORD_DATA);
	int_time[0] = line >> 12;
	int_time[1] = line >> 4;
	int_time[2] = line << 4;
	msm_camera_i2c_write_seq(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->coarse_int_time_addr-1,
		&int_time[0], 3);
	msm_camera_i2c_write(s_ctrl->sensor_i2c_client,
		s_ctrl->sensor_exp_gain_info->global_gain_addr, gain,
		MSM_CAMERA_I2C_WORD_DATA);
	s_ctrl->func_tbl->sensor_group_hold_off(s_ctrl);
	return 0;
#endif
	/* End - jaemoon.hwang@kttech.co.kr */
}

static const struct i2c_device_id s5k4e5_i2c_id[] = {
	{SENSOR_NAME, (kernel_ulong_t)&s5k4e5_s_ctrl},
	{ }
};

static struct i2c_driver s5k4e5_i2c_driver = {
	.id_table = s5k4e5_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_NAME,
	},
};

static struct msm_camera_i2c_client s5k4e5_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};

/* Begin - jaemoon.hwang@kttech.co.kr */
/* CONFIG_CAMERA_CALIBRATION_EEPROM */
#if 1
static const struct i2c_device_id s5k4e5_eeprom_i2c_id[] = {
	{SENSOR_EEPROM_NAME, (kernel_ulong_t)&s5k4e5_eeprom_s_ctrl},
	{ }
};

static struct i2c_driver s5k4e5_eeprom_i2c_driver = {
	.id_table = s5k4e5_eeprom_i2c_id,
	.probe  = msm_sensor_i2c_probe,
	.driver = {
		.name = SENSOR_EEPROM_NAME,
	},
};

static struct msm_camera_i2c_client s5k4e5_eeprom_sensor_i2c_client = {
	.addr_type = MSM_CAMERA_I2C_WORD_ADDR,
};
#endif
/* End - jaemoon.hwang@kttech.co.kr */

static int __init msm_sensor_init_module(void)
{
/* Begin - jaemoon.hwang@kttech.co.kr */
/* CONFIG_CAMERA_CALIBRATION_EEPROM */
#if 1
	i2c_add_driver(&s5k4e5_eeprom_i2c_driver);
#endif
/* End - jaemoon.hwang@kttech.co.kr */
	return i2c_add_driver(&s5k4e5_i2c_driver);
}

static struct v4l2_subdev_core_ops s5k4e5_subdev_core_ops = {
	.ioctl = msm_sensor_subdev_ioctl,
	.s_power = msm_sensor_power,
};

static struct v4l2_subdev_video_ops s5k4e5_subdev_video_ops = {
	.enum_mbus_fmt = msm_sensor_v4l2_enum_fmt,
};

static struct v4l2_subdev_ops s5k4e5_subdev_ops = {
	.core = &s5k4e5_subdev_core_ops,
	.video  = &s5k4e5_subdev_video_ops,
};

static struct msm_sensor_fn_t s5k4e5_func_tbl = {
	.sensor_start_stream = msm_sensor_start_stream,
	.sensor_stop_stream = msm_sensor_stop_stream,
	.sensor_group_hold_on = msm_sensor_group_hold_on,
	.sensor_group_hold_off = msm_sensor_group_hold_off,
	.sensor_set_fps = msm_sensor_set_fps,
	.sensor_write_exp_gain = s5k4e5_write_exp_gain,
	.sensor_write_snapshot_exp_gain = s5k4e5_write_exp_gain,
	.sensor_setting = msm_sensor_setting,
	.sensor_set_sensor_mode = msm_sensor_set_sensor_mode,
	.sensor_mode_init = msm_sensor_mode_init,
	.sensor_get_output_info = msm_sensor_get_output_info,
	.sensor_config = msm_sensor_config,
	.sensor_power_up = msm_sensor_power_up,
	.sensor_power_down = msm_sensor_power_down,
	.sensor_get_csi_params = msm_sensor_get_csi_params,
};

static struct msm_sensor_reg_t s5k4e5_regs = {
	.default_data_type = MSM_CAMERA_I2C_BYTE_DATA,
	.start_stream_conf = s5k4e5_start_settings,
	.start_stream_conf_size = ARRAY_SIZE(s5k4e5_start_settings),
	.stop_stream_conf = s5k4e5_stop_settings,
	.stop_stream_conf_size = ARRAY_SIZE(s5k4e5_stop_settings),
	.group_hold_on_conf = s5k4e5_groupon_settings,
	.group_hold_on_conf_size = ARRAY_SIZE(s5k4e5_groupon_settings),
	.group_hold_off_conf = s5k4e5_groupoff_settings,
	.group_hold_off_conf_size =
		ARRAY_SIZE(s5k4e5_groupoff_settings),
	.init_settings = &s5k4e5_init_conf[0],
	.init_size = ARRAY_SIZE(s5k4e5_init_conf),
	.mode_settings = &s5k4e5_confs[0],
	.output_settings = &s5k4e5_dimensions[0],
	.num_conf = ARRAY_SIZE(s5k4e5_confs),
};

static struct msm_sensor_ctrl_t s5k4e5_s_ctrl = {
	.msm_sensor_reg = &s5k4e5_regs,
	.sensor_i2c_client = &s5k4e5_sensor_i2c_client,
	.sensor_i2c_addr = 0x20,
	.sensor_output_reg_addr = &s5k4e5_reg_addr,
	.sensor_id_info = &s5k4e5_id_info,
	.sensor_exp_gain_info = &s5k4e5_exp_gain_info,
	.cam_mode = MSM_SENSOR_MODE_INVALID,
	.csi_params = &s5k4e5_csi_params_array[0],
//	.csic_params = &s5k4e5_csi_params_array[0],
	.msm_sensor_mutex = &s5k4e5_mut,
	.sensor_i2c_driver = &s5k4e5_i2c_driver,
	.sensor_v4l2_subdev_info = s5k4e5_subdev_info,
	.sensor_v4l2_subdev_info_size = ARRAY_SIZE(s5k4e5_subdev_info),
	.sensor_v4l2_subdev_ops = &s5k4e5_subdev_ops,
	.func_tbl = &s5k4e5_func_tbl,
	.clk_rate = MSM_SENSOR_MCLK_24HZ,
};

/* Begin - jaemoon.hwang@kttech.co.kr */
/* CONFIG_CAMERA_CALIBRATION_EEPROM */
#if 1	// def CONFIG_CALIBRATION_FEATURE
static struct msm_sensor_ctrl_t s5k4e5_eeprom_s_ctrl = {
	.sensor_i2c_client = &s5k4e5_eeprom_sensor_i2c_client,
	.sensor_i2c_addr = 0xA0,	// GT24C16 : original 0xA0
	.sensor_i2c_driver = &s5k4e5_eeprom_i2c_driver,
};
#endif
/* End - jaemoon.hwang@kttech.co.kr */

module_init(msm_sensor_init_module);
MODULE_DESCRIPTION("Samsung LSI 5MP Bayer sensor driver");
MODULE_LICENSE("GPL v2");


