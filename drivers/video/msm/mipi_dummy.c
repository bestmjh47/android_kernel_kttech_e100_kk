/* Copyright (c) 2008-2010, Code Aurora Forum. All rights reserved.
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

static struct msm_panel_common_pdata *mipi_dummy_pdata;

static struct dsi_buf dummy_tx_buf;
static struct dsi_buf dummy_rx_buf;

static int mipi_dummy_lcd_on(struct platform_device *pdev)
{
	return 0;
}

static int mipi_dummy_lcd_off(struct platform_device *pdev)
{
	return 0;
}

static int __devinit mipi_dummy_lcd_probe(struct platform_device *pdev)
{
	if (pdev->id == 0) {
		mipi_dummy_pdata = pdev->dev.platform_data;
		return 0;
	}

	msm_fb_add_device(pdev);

	return 0;
}

static struct platform_driver this_driver = {
	.probe  = mipi_dummy_lcd_probe,
	.driver = {
		.name   = "mipi_dummy",
	},
};

static struct msm_fb_panel_data dummy_panel_data = {
	.on		= mipi_dummy_lcd_on,
	.off		= mipi_dummy_lcd_off,
};

static int ch_used[3];
 
int mipi_dummy_device_register(struct msm_panel_info *pinfo,
					u32 channel, u32 panel)
{
	struct platform_device *pdev = NULL;
	int ret;

	if ((channel >= 3) || ch_used[channel])
		return -ENODEV;

	ch_used[channel] = TRUE;

	pdev = platform_device_alloc("mipi_dummy", (panel << 8)|channel);
	if (!pdev)
		return -ENOMEM;

	dummy_panel_data.panel_info = *pinfo;

	ret = platform_device_add_data(pdev, &dummy_panel_data,
		sizeof(dummy_panel_data));
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

static int __init mipi_dummy_lcd_init(void)
{
	mipi_dsi_buf_alloc(&dummy_tx_buf, DSI_BUF_SIZE);
	mipi_dsi_buf_alloc(&dummy_rx_buf, DSI_BUF_SIZE);

#if 0 //def CONFIG_FB_MSM_MIPI_PANEL_DETECT
        if (msm_fb_detect_client("mipi_video_dummy_wvga"))
                return platform_driver_register(&this_driver);
        return 0;
#else
        return platform_driver_register(&this_driver);
#endif

}

module_init(mipi_dummy_lcd_init);

