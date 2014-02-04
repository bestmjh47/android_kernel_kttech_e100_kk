/* drivers/input/touchscreen/melfas_ts.c
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


#include <linux/module.h>
#include <linux/delay.h>
#include <linux/earlysuspend.h>
#include <linux/hrtimer.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/board_kttech.h>
#include <mach/board.h>

#include <mach/gpio.h>
#include <mach/irqs.h>
#include <mach/pmic.h>

#include "melfas_ts.h"
//#include "MMS100A_Config_Updater.h"


#define TS_MAX_Z_TOUCH			255
#define TS_MAX_W_TOUCH		100


#define TS_MAX_X_COORD 		720
#define TS_MAX_Y_COORD 		1280

#define FW_VERSION				0x28
#define COMP_VERSION			0x41

#define TS_READ_START_ADDR 	0x0F
#define TS_READ_START_ADDR2 	0x10
#define TS_READ_VERSION_ADDR	0xF0

#define TS_READ_REGS_LEN 		66
#define MELFAS_MAX_TOUCH		10
#define MAX_ESD_RETRY			3

#define DEBUG_PRINT 			0


#define SET_DOWNLOAD_BY_GPIO	1

#if SET_DOWNLOAD_BY_GPIO
#include "MMS100A_Config_Updater.h"
struct i2c_client *download_client;
#endif // SET_DOWNLOAD_BY_GPIO

#define I2C_RETRY_CNT		5

#define MELFAS_ON			1
#define MELFAS_OFF			0
#define DEFAULT_FPS			50
#define DEFAULT_MERGE		30
#define DEFAULT_EDIT		0

#define SUSPEND_MODE 1
#define RESUME_MODE 0

struct muti_touch_info
{
	int strength;
	int width;	
	int posX;
	int posY;
};

struct melfas_ts_data 
{
	uint16_t addr;
	struct i2c_client *client; 
	struct input_dev *input_dev;
	struct melfas_tsi_platform_data *pdata;
	struct semaphore	msg_sem;
	struct work_struct  work;
	uint32_t flags;
	uint32_t fps;
	uint32_t merge;
	uint32_t edit;
	uint32_t esd_disable_detection;
	uint32_t esd_test_mode;
	uint32_t esd_counter;
	uint32_t esd_enable_counter;
	unsigned char fw_version;
	unsigned char core_version;
	unsigned char priv_version;
	unsigned char pub_version;
	int (*power)(int on);
	struct early_suspend early_suspend;

	struct delayed_work esd_detect_dwork;
	struct delayed_work enable_esd_detect_dwork;
};

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h);
static void melfas_ts_late_resume(struct early_suspend *h);
#endif

struct workqueue_struct *esd_detect_dwork_wq = NULL;
struct workqueue_struct *enable_esd_detect_dwork_wq = NULL;

atomic_t is_suspend = ATOMIC_INIT(0);
static struct muti_touch_info g_Mtouch_info[MELFAS_MAX_TOUCH];

#ifdef USING_GPIO_FOR_VDD
#ifdef USING_LVS6
static struct regulator *qt602240_reg_lvs6;
#endif
#else
static struct regulator *melfas_reg_l9;
#endif

int melfas_power(int on);
static int melfas_init_panel(struct melfas_ts_data *ts);
/* For sys filesystem */
static unsigned int melfas_debug_point_level = 0;

static void set_fps(struct melfas_ts_data *ts, int val);
static void set_merge(struct melfas_ts_data *ts, int val);
static void set_edit(struct melfas_ts_data *ts, int val);

static ssize_t melfas_debug_point_show(struct device *dev, 
				struct device_attribute *attr, char *buf);
static ssize_t melfas_debug_point_store(struct device *dev, 
				struct device_attribute *attr, const char *buf, size_t count);
static ssize_t melfas_fps_ts_show(struct device *dev,
                            struct device_attribute *attr, char *buf);
static ssize_t melfas_fps_ts_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count);
static ssize_t melfas_esd_ts_show(struct device *dev,
							struct device_attribute *attr, char *buf);
static ssize_t melfas_esd_ts_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count);
static ssize_t melfas_merge_ts_show(struct device *dev,
                            struct device_attribute *attr, char *buf);
static ssize_t melfas_merge_ts_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count);
static ssize_t melfas_edit_ts_show(struct device *dev,
                            struct device_attribute *attr, char *buf);
static ssize_t melfas_edit_ts_store(struct device *dev,
                struct device_attribute *attr, const char *buf, size_t count);
static ssize_t melfas_version_show(struct device *dev, 
				struct device_attribute *attr, char *buf);

static DEVICE_ATTR(version, 0444, melfas_version_show, NULL);
static DEVICE_ATTR(debug_point, 0600, melfas_debug_point_show, melfas_debug_point_store);
static DEVICE_ATTR(fps_ts, 0600, melfas_fps_ts_show, melfas_fps_ts_store);
static DEVICE_ATTR(esd_ts, 0600, melfas_esd_ts_show, melfas_esd_ts_store);
static DEVICE_ATTR(merge_ts, 0600, melfas_merge_ts_show, melfas_merge_ts_store);
static DEVICE_ATTR(edit_ts, 0600, melfas_edit_ts_show, melfas_edit_ts_store);

static struct attribute *melfas_attrs[] = {
	&dev_attr_version.attr,
	&dev_attr_debug_point.attr,
	&dev_attr_fps_ts.attr,
	&dev_attr_esd_ts.attr,
	&dev_attr_merge_ts.attr,
	&dev_attr_edit_ts.attr,
	NULL
};

static const struct attribute_group melfas_attr_group = {
	.attrs = melfas_attrs,
};

static int melfas_read_bytes(struct i2c_client *client, uint8_t addr, uint8_t *buf, uint16_t length)
{
	struct melfas_ts_data *ts;
	int ret, i;

	ts = i2c_get_clientdata(client);

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		return 0;

	for(i=0; i<I2C_RETRY_CNT; i++){
		buf[0] = addr;
		/* Send to read address */
		ret = i2c_master_send(client, buf, 1);
		if(ret < 0) {
			printk("[melfas] I2C master-send failed.(%d)\n", ret);
		}
		else {
		/* Receive to read data */
		ret = i2c_master_recv(client, buf, length);
		if(ret != length) {
			printk("[melfas] I2C master-receive failed.(%d)\n", ret);
		}
		else
			break;
		}
	}
	return ret;
}

static int melfas_write_bytes(struct i2c_client *client, uint8_t addr, uint8_t *val, uint16_t length)
{
	struct melfas_ts_data *ts;
	int ret, i;

	struct {
		uint8_t i2c_addr;
		uint8_t buf[256];
	} i2c_block_transfer;

	ts = i2c_get_clientdata(client);

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		return 0;

	i2c_block_transfer.i2c_addr = addr;

	for(i=0; i < length; i++)
		i2c_block_transfer.buf[i] = *val++;

	for(i=0; i<I2C_RETRY_CNT; i++){
		/* Send to write address */
		ret = i2c_master_send(client, (u8 *)&i2c_block_transfer, length+1);
		if(ret < 0)
			printk("[melfas] I2C master-send failed.(%d)\n", ret);
	}
	return ret;
}

static ssize_t melfas_version_show(struct device *dev, 
				struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
    int count = 0;

	count += sprintf(buf+count, "F/W version  : %x\n", ts->fw_version);
	count += sprintf(buf+count, "Core version : %x\n", ts->core_version);
	count += sprintf(buf+count, "Priv version : %x\n", ts->priv_version);
	count += sprintf(buf+count, "Pub version  : %x\n", ts->pub_version);

	return count;
}
static ssize_t melfas_debug_point_show(struct device *dev, 
								struct device_attribute *attr, char *buf)
{
	int count = 0;

	count += sprintf(buf+count, "1 : DEBUG_LOW_POSITION\n\n");
	count += sprintf(buf+count, "Debug Point Level : %x\n", melfas_debug_point_level);

	return count;
}

static ssize_t melfas_debug_point_store(struct device *dev, 
				struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int level = 0;

	if (sscanf(buf, "%u", &level) != 1) {
		printk("Invalid values\n");
		return -EINVAL;
	}

	switch(level){
		case 0:
			level = 0;
			break;
		case 1:
			level = (melfas_debug_point_level | DEBUG_LOW_POSITION);
			break;
		default:
			level = 0;
			break;
	}

	melfas_debug_point_level = level;

	return count;
}

static ssize_t melfas_fps_ts_show(struct device *dev,
                            struct device_attribute *attr, char *buffer)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
    int count = 0;

    count += sprintf(buffer+count, "FPS : %d \n", ts->fps);
	
    return count;
}

static ssize_t melfas_fps_ts_store(struct device *dev,
                struct device_attribute *attr, const char *buffer, size_t count)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
	int val;

	if(sscanf(buffer, "%d", &val) != 1) {
		printk("melfas : Invalid value\n");
		return count;
	}

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		goto out;

	if(val < 30)
		val = 30;
	if(val > 150)
		val = 150;

	set_fps(ts, val);

out:
	return count;
}

static ssize_t melfas_esd_ts_show(struct device *dev,
							struct device_attribute *attr, char *buf)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buf+count, "ESD : %d \nCount : %d\n", 
							ts->esd_test_mode, ts->esd_enable_counter);

	return count;
}

static ssize_t melfas_esd_ts_store(struct device *dev,
				struct device_attribute *attr, const char *buf, size_t count)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
	int val;

	if(sscanf(buf, "%d", &val) != 1) {
//		printk("namjja : Invalid value\n");
		return count;
	}

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		goto out;

	if(val)
		ts->esd_test_mode = 1;
out:
	return count;
}

static ssize_t melfas_merge_ts_show(struct device *dev,
                            struct device_attribute *attr, char *buffer)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
    int count = 0;

    count += sprintf(buffer+count, "FPS : %d \n", ts->merge);
	
    return count;
}

static ssize_t melfas_merge_ts_store(struct device *dev,
                struct device_attribute *attr, const char *buffer, size_t count)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
	int val;

	if(sscanf(buffer, "%d", &val) != 1) {
//		printk("namjja : Invalid value\n");
		return count;
	}

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		goto out;

	if(val < 10)
		val = 10;
	if(val > 60)
		val = 60;

	set_merge(ts, val);

out:
	return count;
}

static ssize_t melfas_edit_ts_show(struct device *dev,
                            struct device_attribute *attr, char *buffer)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
	int count = 0;

	count += sprintf(buffer+count, "Edit : %d \n", ts->edit);

	return count;
}

static ssize_t melfas_edit_ts_store(struct device *dev,
                struct device_attribute *attr, const char *buffer, size_t count)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)dev_get_drvdata(dev);
	int val;

	if(sscanf(buffer, "%d", &val) != 1) {
//		printk("namjja : Invalid value\n");
		return count;
	}

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		goto out;

	if(val)
		val = 1;
	else
		val = 0;

	set_edit(ts, val);

out:
	return count;
}

/* Disable ESD Detection logic */
static void disable_esd_detection(struct melfas_ts_data *ts)
{
	uint8_t buf_ts[TS_READ_REGS_LEN];

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		return;

	buf_ts[0] = TS_CMD_SET_ESD;
	buf_ts[1] = 0x1;

	if(melfas_write_bytes(ts->client, TS_VNDR_CMDID, buf_ts, 2) < 0) {
		printk("[melfas] Disable ESD Detection set failed.\n");
	}

	if(melfas_read_bytes(ts->client, TS_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		printk("[melfas] Disable ESD Detection set result read failed.\n");
	}

	if(buf_ts[0] != 0x1) {
		printk("[melfas] Disable ESD Detection set failed.(%d)\n", buf_ts[0]);
		return;
	}
	else
		printk("[melfas] Disable ESD Detection set succeed.(%d)\n", buf_ts[0]);

	ts->esd_disable_detection = 1;

	if(ts->esd_enable_counter < MAX_ESD_RETRY && !delayed_work_pending(&ts->enable_esd_detect_dwork)) {
		queue_delayed_work(enable_esd_detect_dwork_wq, &ts->enable_esd_detect_dwork, ENABLE_ESD_DETECT_LIMIT_TIME);
		printk("[melfas] ESD Detection %d ms after enable (%d)\n",
				ENABLE_ESD_DETECT_LIMIT_TIME, ts->esd_enable_counter);
	}
	return;
}

/* Disable ESD Detection logic */
static void enable_esd_detection_dworker(struct work_struct *work)
{
	struct melfas_ts_data *ts;
	uint8_t buf_ts[TS_READ_REGS_LEN];

	ts = container_of(work, struct melfas_ts_data, enable_esd_detect_dwork.work);

	if(atomic_read(&is_suspend) == SUSPEND_MODE)
		return;

	buf_ts[0] = TS_CMD_SET_ESD;
	buf_ts[1] = 0x0;

	if(melfas_write_bytes(ts->client, TS_VNDR_CMDID, buf_ts, 2) < 0) {
		printk("[melfas] enable ESD Detection set failed.\n");
	}

	if(melfas_read_bytes(ts->client, TS_VNDR_CMD_RESULT, buf_ts, 1)< 0) {
		printk("[melfas] enable ESD Detection set result read failed.\n");
	}

	if(buf_ts[0] != 0x1) {
		printk("[melfas] enable ESD Detection set failed.(%d)\n", buf_ts[0]);
		return;
	}
	else
		printk("[melfas] enable ESD Detection set succeed.(%d)\n", buf_ts[0]);

	ts->esd_enable_counter++;
	ts->esd_disable_detection = 0;
	ts->esd_test_mode = 0;

#if 0
	if(ts->esd_enable_counter > 10)
		ts->esd_enable_counter = 0;
#endif

	return;
}

/* ESD Detection logic */
static void esd_detect_dworker(struct work_struct *work)
{
	struct melfas_ts_data *ts;
	ts = container_of(work, struct melfas_ts_data, esd_detect_dwork.work);

	ts->esd_counter++;

	/* Recovery TSP */
	melfas_power(MELFAS_OFF);
	msleep(500);
	melfas_power(MELFAS_ON);

	melfas_init_panel(ts);
	/* Print Debugging information */
	printk("[melfas] ESD Detection logic. TSP Reset completed.\n");

	/* Limit exceed. Please. Check the TSP Panel */
	if(ts->esd_counter > ESD_RETRY_COUNTER_LIMIT) {
		printk("[melfas] ESD Detection limit exceed. ESD Detection disabled.\n");
		/* Disable ESD Detection */
		disable_esd_detection(ts);

		ts->esd_counter = 0;
	}

	return;
}

#if SET_DOWNLOAD_BY_GPIO
int melfas_write(uint8_t slave_addr, uint8_t* buffer, int packet_len)
{
	int ret;
	struct i2c_adapter *adap = download_client->adapter;
	struct i2c_msg msg;

	msg.addr = slave_addr;
	msg.flags = download_client->flags & I2C_M_TEN;
	msg.len = packet_len;
	msg.buf = (char *)buffer;

	ret = i2c_transfer(adap, &msg, 1);

	return (ret == 1);
}

int melfas_read(uint8_t slave_addr, uint8_t* buffer, int packet_len)
{
	struct i2c_adapter *adap = download_client->adapter;
	struct i2c_msg msg;
	int ret;

	msg.addr = slave_addr;
	msg.flags = download_client->flags & I2C_M_TEN;
	msg.flags |= I2C_M_RD;
	msg.len = packet_len;
	msg.buf = buffer;

	ret = i2c_transfer(adap, &msg, 1);

	return (ret == 1);
}
#endif

static int read_result(struct melfas_ts_data *ts)
{
	uint8_t buf[1];

	if(melfas_read_bytes(ts->client, TS_VNDR_CMD_RESULT, buf, 1) <0)
	{
		printk(KERN_ERR "%s: i2c_master_recv() failed\n", __func__);
	}
	
	printk("melfas result %d\n", buf[0]);

	return buf[0] == 1 ? 1 : 0;
}

static void set_edit(struct melfas_ts_data *ts, int val)
{
	uint8_t buf[6];

	buf[0] = TS_CMD_SET_EDIT;
	buf[1] = (val & 0xff);

	if(melfas_write_bytes(ts->client, TS_VNDR_CMDID, buf, 2) <0)
	{
		printk(KERN_ERR "%s: i2c_master_send() failed\n", __func__);
	}

	if(read_result(ts))
		ts->edit = (val & 0xff);
}

static void set_merge(struct melfas_ts_data *ts, int val)
{
	uint8_t buf[6];

	buf[0] = TS_CMD_SET_MERGE;
	buf[1] = (val & 0xff);

	if(melfas_write_bytes(ts->client, TS_VNDR_CMDID, buf, 2) <0)
	{
		printk(KERN_ERR "%s: i2c_master_send() failed\n", __func__);
	}

	if(read_result(ts))
		ts->merge = (val & 0xff);
}

static void set_fps(struct melfas_ts_data *ts, int val)
{
	uint8_t buf[6];

	buf[0] = TS_CMD_SET_FRAMERATE;
	buf[1] = (val & 0xff);

	if(melfas_write_bytes(ts->client, TS_VNDR_CMDID, buf, 2) <0)
	{
		printk(KERN_ERR "%s: i2c_master_send() failed\n", __func__);
	}

	if(read_result(ts))
		ts->fps = (val & 0xff);
}

static int melfas_init_gpio(void)
{
	int rc = 0;

#ifdef USING_GPIO_FOR_VDD
	 gpio_tlmm_config(GPIO_CFG(TSP_3_3V_EN, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

	if (gpio_request(TSP_3_3V_EN, "ts_en") < 0){
		printk("melfas : ts_en failed\n");
		goto gpio_failed;
	}
//	printk("melfas : power on\n");

	rc = gpio_direction_output(TSP_3_3V_EN, 1);
#endif

	if(gpio_request(MELFAS_TSP_INT, "ts_int") < 0){
		printk("melfas : ts_int failed\n");
		goto gpio_failed;
	}

	gpio_tlmm_config(GPIO_CFG(MELFAS_TSP_INT, 0, GPIO_CFG_INPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	rc = gpio_direction_input(MELFAS_TSP_INT);
//	printk("melfas : int gpio en\n");
	return rc;
gpio_failed:
	printk("melfas : GPIO Request failed\n");
	return -EINVAL;
}

int melfas_power(int on)
{
	int rc = 0;
	if (on) {

#ifdef USING_GPIO_FOR_VDD
		gpio_set_value(TSP_3_3V_EN, 1);
		
#ifdef USING_LVS6
		if (qt602240_reg_lvs6)
			return rc;

		qt602240_reg_lvs6 = regulator_get(NULL, "8921_lvs6");

		rc = regulator_enable(qt602240_reg_lvs6);
		if (rc) {
			printk("melfas : Regulator Enable (LVS6) failed! (%d)\n", rc);
			regulator_put(qt602240_reg_lvs6);
			return rc;
		}
#endif
#else
		melfas_reg_l9 =  regulator_get(NULL, "8921_l9");

		rc = regulator_set_voltage(melfas_reg_l9, 3300000, 3300000);
		if (!rc){
			rc = regulator_enable(melfas_reg_l9);
//			printk("namjja : power on\n");
			msleep(500);
		}else{
			printk("namjja : failed power\n");
		}
#endif

		msleep(10);

		gpio_tlmm_config(GPIO_CFG(MELFAS_TSP_SCL, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(MELFAS_TSP_SDA, 1, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_8MA), GPIO_CFG_ENABLE);

		return rc;
	} else {
#if 1
		gpio_tlmm_config(GPIO_CFG(MELFAS_TSP_SCL, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(MELFAS_TSP_SDA, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
#else
		gpio_tlmm_config(GPIO_CFG(MELFAS_TSP_SCL, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		gpio_tlmm_config(GPIO_CFG(MELFAS_TSP_SDA, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_DOWN, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
		msleep(10);
		gpio_set_value(MELFAS_TSP_SCL, 1);
		gpio_set_value(MELFAS_TSP_SDA, 1);
#endif
		msleep(10);
#ifdef USING_GPIO_FOR_VDD
		gpio_set_value(TSP_3_3V_EN, 0);

#ifdef USING_LVS6
		if (!qt602240_reg_lvs6)
			return rc;

		rc = regulator_disable(qt602240_reg_lvs6);
		if(rc) {
			printk("melfas : Regulator Disable (LVS6) failed! (%d)\n", rc);
			return rc;
		}

		regulator_put(qt602240_reg_lvs6);
		qt602240_reg_lvs6 = NULL;
#endif

#else
		rc = regulator_disable(melfas_reg_l9);
#endif
		return rc;
	}
}

//#ifndef CONFIG_HAS_EARLYSUSPEND
static int melfas_init_panel(struct melfas_ts_data *ts)
{
	char buf = 0x00;
	int ret;
	int i;

	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)  /* _SUPPORT_MULTITOUCH_ */
		g_Mtouch_info[i].strength = 0;	

    input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);

	ret = i2c_master_send(ts->client, &buf, 1);

	if(ret <0)
	{
		printk(KERN_ERR "melfas_ts_probe: i2c_master_send() failed\n [%d]", ret);	
		return 0;
	}

	return true;
}
//#endif

static void melfas_ts_get_data(struct melfas_ts_data *ts)
{
	int ret = 0, i;
	uint8_t buf[TS_READ_REGS_LEN];
	int read_num, FingerID;
	int touchType;
	int chkpress = 0;


#if DEBUG_PRINT
	printk(KERN_ERR "melfas_ts_work_func\n");

#endif 

	ret = melfas_read_bytes(ts->client, TS_READ_START_ADDR, buf, 1);
	if(ret < 0)
	{
#if DEBUG_PRINT
		printk(KERN_ERR "melfas_ts_work_func: i2c failed\n");
		return ;	
#endif 
	}

	read_num = buf[0];
	if(read_num > TS_READ_REGS_LEN)
		read_num = TS_READ_REGS_LEN;
	
	if(read_num>0)
	{
		ret = melfas_read_bytes(ts->client, TS_READ_START_ADDR2, buf, read_num);
		if(ret < 0)
		{
#if DEBUG_PRINT
			printk(KERN_ERR "melfas_ts_work_func: i2c failed\n");
			return ;	
#endif 
		}

		for(i=0; i<read_num; i=i+6)
		{
			FingerID = (buf[i] & 0x0F)-1;
			touchType = (buf[i] & 0x60)>>5;

			if(ts->esd_test_mode || (touchType == TOUCH_TYPE_NONE && FingerID >= MELFAS_MAX_TOUCH)) {
				printk("melfas : EDS Detection\n");
				ts->esd_test_mode = 0;
				if(!delayed_work_pending(&ts->esd_detect_dwork))
					queue_delayed_work(esd_detect_dwork_wq, &ts->esd_detect_dwork, 0);
				return;
			}
			if(FingerID >= MELFAS_MAX_TOUCH){
				printk("melfas : FingerID error\n");
				return;
			}

			g_Mtouch_info[FingerID].posX= (uint16_t)(buf[i+1] & 0x0F) << 8 | buf[i+2];
			g_Mtouch_info[FingerID].posY= (uint16_t)(buf[i+1] & 0xF0) << 4 | buf[i+3];	
			
			if((buf[i] & 0x80)==0)
				g_Mtouch_info[FingerID].strength = 0;
			else
				g_Mtouch_info[FingerID].strength = buf[i+4];
			
			g_Mtouch_info[FingerID].width= buf[i+5];					

		}
	
	}

	if (ret < 0)
	{
		printk(KERN_ERR "melfas_ts_work_func: i2c failed\n");
		return ;	
	}
	else 
	{

		for(i=0; i<MELFAS_MAX_TOUCH; i++)
		{
			if(g_Mtouch_info[i].strength== 0)
				continue;
			
			input_report_abs(ts->input_dev, ABS_MT_TRACKING_ID, i);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_X, g_Mtouch_info[i].posX);
			input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, g_Mtouch_info[i].posY);
			input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, g_Mtouch_info[i].strength );
			input_report_abs(ts->input_dev, ABS_MT_WIDTH_MAJOR, g_Mtouch_info[i].width);      				
			input_mt_sync(ts->input_dev);          
			if(g_Mtouch_info[i].strength > 0)
				chkpress++;

			if (melfas_debug_point_level & DEBUG_LOW_POSITION) {
				printk("melfas_ts_work_func: Touch ID: %d, State : %d, x: %d, y: %d, z: %d w: %d\n", 
					i, (g_Mtouch_info[i].strength>0), g_Mtouch_info[i].posX, g_Mtouch_info[i].posY, g_Mtouch_info[i].strength, g_Mtouch_info[i].width);
			}

#if 0
			if(g_Mtouch_info[i].strength == 0)
				g_Mtouch_info[i].strength = -1;
#endif
		}
#if DEBUG_PRINT
		printk("namjja : release [%d]\n", !!chkpress);
#endif
		input_report_key(ts->input_dev, BTN_TOUCH, !!chkpress);
			
		input_sync(ts->input_dev);
	}
			
}

static irqreturn_t melfas_ts_irq_handler(int irq, void *handle)
{
	struct melfas_ts_data *ts = (struct melfas_ts_data *)handle;
#if DEBUG_PRINT
	printk(KERN_ERR "melfas : melfas_ts_irq_handler\n");
#endif

	if (down_interruptible(&ts->msg_sem)) { 
		printk("melfas_ts_irq_handler Interrupted "
					"while waiting for msg_sem!\n");
	}
	else {
		melfas_ts_get_data(ts);
		up(&ts->msg_sem);
	}
	return IRQ_HANDLED;
}

static int melfas_ftm_mode_probe(struct i2c_client *client)
{
	struct input_dev *input_dev;
	int ret = 0;

	input_dev = input_allocate_device();
    if (!input_dev)
    {
		printk(KERN_ERR "melfas_ftm_mode_probe : Not enough memory\n");
		ret = -ENOMEM;
		return ret;
	} 
	input_dev->name = "melfas-ts" ;

	input_dev->keybit[BIT_WORD(KEY_MENU)] |= BIT_MASK(KEY_MENU);
	input_dev->keybit[BIT_WORD(KEY_HOME)] |= BIT_MASK(KEY_HOME);
	input_dev->keybit[BIT_WORD(KEY_BACK)] |= BIT_MASK(KEY_BACK);		
	input_dev->keybit[BIT_WORD(KEY_SEARCH)] |= BIT_MASK(KEY_SEARCH);			


	__set_bit(BTN_TOUCH, input_dev->keybit);
	__set_bit(EV_ABS,  input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, input_dev->propbit);

	input_set_abs_params(input_dev, ABS_MT_POSITION_X, 0, TS_MAX_X_COORD, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y, 0, TS_MAX_Y_COORD, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR, 0, TS_MAX_Z_TOUCH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_WIDTH_MAJOR, 0, TS_MAX_W_TOUCH, 0, 0);
	__set_bit(EV_SYN, input_dev->evbit); 
	__set_bit(EV_KEY, input_dev->evbit);	


    ret = input_register_device(input_dev);
	if(ret)
		return ret;
	ret = sysfs_create_group(&client->dev.kobj, &melfas_attr_group);

	return ret;
}

static int melfas_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct melfas_ts_data *ts;
	int ret = 0, i; 
	
	uint8_t buf[6] = {0,};
	
//	printk("namjja : %s\n", __func__);
#if DEBUG_PRINT
	printk(KERN_ERR "kim ms : melfas_ts_probe\n");
#endif

	if(get_kttech_ftm_mode() == FTM_MODE_NO_LCD){
		printk("melfas : FTM mode\n");
		ret = melfas_ftm_mode_probe(client);
		if(ret)
			printk("melfas : FTM error : %d\n", ret);
		return 0;
	}
	melfas_power(MELFAS_ON);
	
	melfas_init_gpio();
	msleep(500);

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C))
    {
        printk(KERN_ERR "melfas_ts_probe: need I2C_FUNC_I2C\n");
        ret = -ENODEV;
        goto err_check_functionality_failed;
    }

    ts = kmalloc(sizeof(struct melfas_ts_data), GFP_KERNEL);
    if (ts == NULL)
    {
        printk(KERN_ERR "melfas_ts_probe: failed to create a state of melfas-ts\n");
        ret = -ENOMEM;
        goto err_alloc_data_failed;
    }

    ts->client = client;
    i2c_set_clientdata(client, ts);

#if DEBUG_PRINT
	printk(KERN_ERR "melfas_ts_probe: i2c_master_send() [%d], Add[%d]\n", ret, ts->client->addr);
#endif

	ts->esd_counter = 0;
	ts->esd_disable_detection = 0;
	ts->esd_enable_counter = 0;
	ts->esd_test_mode = 0;

#if SET_DOWNLOAD_BY_GPIO
	ret = melfas_read_bytes(ts->client, TS_READ_VERSION_ADDR, buf, 6);
	printk(KERN_ERR "melfas_ts_probe : F/W version [%x]\n", buf[0]);
	ts->fw_version = buf[0];
	printk(KERN_ERR "melfas_ts_probe : H/W version [%x]\n", buf[1]);
	printk(KERN_ERR "melfas_ts_probe : Comp F/W version [%x]\n", buf[2]);
	printk(KERN_ERR "melfas_ts_probe : Core F/W version [%x]\n", buf[3]);
	ts->core_version = buf[3];
	printk(KERN_ERR "melfas_ts_probe : Priv F/W version [%x]\n", buf[4]);
	ts->priv_version = buf[4];
	printk(KERN_ERR "melfas_ts_probe : Pub F/W version [%x]\n", buf[5]);
	ts->pub_version = buf[5];

	if(ret < 0)
	{
		printk(KERN_ERR "melfas_ts_probe : i2c_master_recv [%d]\n", ret);			
	}
#if 1
	download_client = ts->client;
	if((buf[0] < FW_VERSION && buf[2] == COMP_VERSION) || (buf[0] == 0))
	{
		ret = MFS_config_update(0x48);
		if(ret != 0)
		{
			printk(KERN_ERR "SET Download Fail - error code [%d]\n", ret);			
		}
	}else{
		printk("Not Upgrade %x %x\n", buf[0], buf[2]);
	}	
#endif
#endif // SET_DOWNLOAD_BY_GPIO
	
	ts->input_dev = input_allocate_device();
    if (!ts->input_dev)
    {
		printk(KERN_ERR "melfas_ts_probe: Not enough memory\n");
		ret = -ENOMEM;
		goto err_input_dev_alloc_failed;
	} 

	set_fps(ts, DEFAULT_FPS);
	set_merge(ts, DEFAULT_MERGE);
	set_edit(ts, DEFAULT_EDIT);
	ts->input_dev->name = "melfas-ts" ;

	//ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	

	ts->input_dev->keybit[BIT_WORD(KEY_MENU)] |= BIT_MASK(KEY_MENU);
	ts->input_dev->keybit[BIT_WORD(KEY_HOME)] |= BIT_MASK(KEY_HOME);
	ts->input_dev->keybit[BIT_WORD(KEY_BACK)] |= BIT_MASK(KEY_BACK);		
	ts->input_dev->keybit[BIT_WORD(KEY_SEARCH)] |= BIT_MASK(KEY_SEARCH);			


	__set_bit(BTN_TOUCH, ts->input_dev->keybit);
	__set_bit(EV_ABS,  ts->input_dev->evbit);
	__set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);	
//	ts->input_dev->evbit[0] =  BIT_MASK(EV_SYN) | BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);	

	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, TS_MAX_X_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, TS_MAX_Y_COORD, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, TS_MAX_Z_TOUCH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TRACKING_ID, 0, MELFAS_MAX_TOUCH, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_WIDTH_MAJOR, 0, TS_MAX_W_TOUCH, 0, 0);
	__set_bit(EV_SYN, ts->input_dev->evbit); 
	__set_bit(EV_KEY, ts->input_dev->evbit);	


    ret = input_register_device(ts->input_dev);
    if (ret)
    {
        printk(KERN_ERR "melfas_ts_probe: Failed to register device\n");
        ret = -ENOMEM;
        goto err_input_register_device_failed;
    }

	INIT_DELAYED_WORK(&ts->esd_detect_dwork, esd_detect_dworker);
	INIT_DELAYED_WORK(&ts->enable_esd_detect_dwork, enable_esd_detection_dworker);
	esd_detect_dwork_wq = create_singlethread_workqueue("tsp_esd_detect");
	if (!esd_detect_dwork_wq)
		goto err_input_register_device_failed;

	enable_esd_detect_dwork_wq = create_singlethread_workqueue("tsp_enable_esd_detect");
	if (!enable_esd_detect_dwork_wq)
		goto err_input_register_device_failed;

    if (ts->client->irq)
    {
#if DEBUG_PRINT
        printk(KERN_ERR "melfas_ts_probe: trying to request irq: %s-%d\n", ts->client->name, ts->client->irq);
#endif
       
//	ret = request_threaded_irq(client->irq, NULL, melfas_ts_irq_handler,IRQF_TRIGGER_FALLING, ts->client->name, ts);
	sema_init(&ts->msg_sem, 1);
	ret = request_threaded_irq(client->irq, NULL, melfas_ts_irq_handler,IRQF_TRIGGER_LOW | IRQF_ONESHOT, ts->client->name, ts);

        if (ret > 0)
        {
            printk(KERN_ERR "melfas_ts_probe: Can't allocate irq %d, ret %d\n", ts->client->irq, ret);
            ret = -EBUSY;
            goto err_request_irq;
        }
    }

	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)  /* _SUPPORT_MULTITOUCH_ */
		g_Mtouch_info[i].strength = 0;	

#if DEBUG_PRINT	
	printk(KERN_ERR "melfas_ts_probe: succeed to register input device\n");
#endif

#if CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = melfas_ts_early_suspend;
	ts->early_suspend.resume = melfas_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif
	
	ret = sysfs_create_group(&client->dev.kobj, &melfas_attr_group);
	if (ret)
		goto err_check_functionality_failed;
		
#if DEBUG_PRINT
	printk(KERN_INFO "melfas_ts_probe: Start touchscreen. name: %s, irq: %d\n", ts->client->name, ts->client->irq);
#endif
	return 0;

err_request_irq:
	printk(KERN_ERR "melfas-ts: err_request_irq failed\n");
	free_irq(client->irq, ts);
err_input_register_device_failed:
	printk(KERN_ERR "melfas-ts: err_input_register_device failed\n");
	input_free_device(ts->input_dev);
err_input_dev_alloc_failed:
	printk(KERN_ERR "melfas-ts: err_input_dev_alloc failed\n");
err_alloc_data_failed:
	printk(KERN_ERR "melfas-ts: err_alloc_data failed_\n");	
//err_detect_failed:
	printk(KERN_ERR "melfas-ts: err_detect failed\n");
	kfree(ts);
err_check_functionality_failed:
	printk(KERN_ERR "melfas-ts: err_check_functionality failed_\n");

	return ret;
}

static int melfas_ts_remove(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	unregister_early_suspend(&ts->early_suspend);
	free_irq(client->irq, ts);
	//ts->power(false);  // modified
	sysfs_remove_group(&client->dev.kobj, &melfas_attr_group);
	melfas_power(MELFAS_OFF);
	input_unregister_device(ts->input_dev);
	kfree(ts);
	return 0;
}

//#ifndef CONFIG_HAS_EARLYSUSPEND
static int melfas_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	int ret;
	int i;    
//namjja
//	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	for (i = 0; i < MELFAS_MAX_TOUCH ; i++)
	{
		g_Mtouch_info[i].strength = 0;
		g_Mtouch_info[i].posX = 0;
		g_Mtouch_info[i].posY = 0;
		g_Mtouch_info[i].width = 0;
	}
    
	disable_irq(client->irq);

//namjja
//	ret = cancel_work_sync(&ts->work);
//	if (ret) /* if work was pending disable-count is now 2 */
//		enable_irq(client->irq);

	ret = i2c_smbus_write_byte_data(client, 0x01, 0x00); /* deep sleep */
	
	if (ret < 0)
		printk(KERN_ERR "melfas_ts_suspend: i2c_smbus_write_byte_data failed\n");

	return 0;
}

static int melfas_ts_resume(struct i2c_client *client)
{
	struct melfas_ts_data *ts = i2c_get_clientdata(client);

	melfas_init_panel(ts);
	enable_irq(client->irq); // scl wave

	return 0;
}
//#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void melfas_ts_early_suspend(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	atomic_set(&is_suspend, SUSPEND_MODE);

	/* Cancel Workqueue */
	if(delayed_work_pending(&ts->enable_esd_detect_dwork))
		cancel_delayed_work_sync(&ts->enable_esd_detect_dwork);

	if(delayed_work_pending(&ts->esd_detect_dwork))
		cancel_delayed_work_sync(&ts->esd_detect_dwork);

	melfas_ts_suspend(ts->client, PMSG_SUSPEND);
	melfas_power(MELFAS_OFF);
	msleep(50);
	input_report_key(ts->input_dev, BTN_TOUCH, 0);
	input_sync(ts->input_dev);
}

static void melfas_ts_late_resume(struct early_suspend *h)
{
	struct melfas_ts_data *ts;
	melfas_power(MELFAS_ON);
	msleep(50);
	ts = container_of(h, struct melfas_ts_data, early_suspend);
	melfas_ts_resume(ts->client);
	atomic_set(&is_suspend, RESUME_MODE);
	if(ts->esd_disable_detection == 1)
		disable_esd_detection(ts);
}
#endif

static const struct i2c_device_id melfas_ts_id[] =
{
    { "melfas-ts", 0 },
    { }
};

static struct i2c_driver melfas_ts_driver =
{
    .driver = {
    .name = "melfas-ts",
    },
    .id_table = melfas_ts_id,
    .probe = melfas_ts_probe,
    .remove = __devexit_p(melfas_ts_remove),
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend		= melfas_ts_suspend,
	.resume		= melfas_ts_resume,
#endif
};

static int __devinit melfas_ts_init(void)
{
	return i2c_add_driver(&melfas_ts_driver);
}

static void __exit melfas_ts_exit(void)
{
	i2c_del_driver(&melfas_ts_driver);
}

MODULE_DESCRIPTION("Driver for Melfas MTSI Touchscreen Controller");
MODULE_AUTHOR("MinSang, Kim <kimms@melfas.com>");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");

module_init(melfas_ts_init);
module_exit(melfas_ts_exit);
