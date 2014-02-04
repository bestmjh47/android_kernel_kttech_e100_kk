#include <linux/module.h>
#include <linux/param.h>
#include <linux/jiffies.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/platform_device.h>
#include <linux/power_supply.h>
#include <linux/idr.h>
#include <linux/i2c.h>
#include <linux/slab.h>
#include <asm/unaligned.h>
#include <linux/kernel.h>
#include <linux/timer.h>

#ifdef CONFIG_MACH_KTTECH
#include <mach/board.h>
#endif

#include <linux/mfd/pm8xxx/pm8921-charger.h>

//void max17040_init(void);
int max17040_get_soc(int batt_temp);
int max17040_get_voltage(void);
//int max17040_get_current(void);
int max17040_get_temperature(void);

#ifdef CONFIG_KTTECH_BATTERY_GAUGE_MAXIM

/**************************************************************************
 * KTTECH_MAXIM_BATT_MODEL_18BIT
 *
 * RCOMP = 0xB700, FullSoc = 98.45, EmptySoc = 1.68, 18bit mode
 *
 * 업체에서 전달 받은 배터리 모델 값이 19BIT일 경우 Feature 삭제
***************************************************************************/
//#define KTTECH_MAXIM_BATT_MODEL_18BIT 

static int maxim_read_reg(u8 regoffset, u8 *value);

#define MAX17410_SLAVE_ADDR			0x36
#define MAX17410_QUP_I2C_BUS_ID	1

#define MAX17410_I2C_RETRY_NUM		16

#define MAX17040_VCELL_MSB	0x02
#define MAX17040_VCELL_LSB	0x03
#define MAX17040_SOC_MSB	0x04
#define MAX17040_SOC_LSB	0x05
#define MAX17040_MODE_MSB	0x06
#define MAX17040_MODE_LSB	0x07
#define MAX17040_VER_MSB	0x08
#define MAX17040_VER_LSB	0x09
#define MAX17040_RCOMP_MSB	0x0C
#define MAX17040_RCOMP_LSB	0x0D
#define MAX17040_OCV_MSB    0x0E
#define MAX17040_OCV_LSB      0x0F
#define MAX17040_CMD_MSB	0xFE
#define MAX17040_CMD_LSB	0xFF

#if 0
#define MAXIM_FGAUGE_MSG_LVL KERN_INFO"[MAXIM Fual gauge] : "
#define MAXIM_FGAUGE_ERR_LVL KERN_ERR"[### MAXIM Fual gauge error!!! ###] : "
#define MAXIM_FGAUGE_MSG(_fmt_,_arg_...) printk(MAXIM_FGAUGE_MSG_LVL _fmt_,##_arg_)
#define MAXIM_FGAUGE_ERR(_fmt_,_arg_...) printk(MAXIM_FGAUGE_ERR_LVL _fmt_,##_arg_)
#else
#define MAXIM_FGAUGE_MSG(_fmt_,_arg_...)
#define MAXIM_FGAUGE_ERR(_fmt_,_arg_...)  printk(KERN_ERR _fmt_,##_arg_)
#endif

static unsigned char m_batt_not_present = 0;
static unsigned char m_latest_rcomp_msb = 0x70;
struct timer_list m_batt_insert_check_timer;

static void max17040_batt_insert_check_func(unsigned long data)
{
	//m_batt_insert_check_timer.expires = jiffies + (HZ*2);
	//add_timer(&m_batt_insert_check_timer);
	m_batt_not_present = 1;
	printk("[gun]max17040_batt_insert_check_func() => No Battery!\n");
	pm8921_update_battery_status_force();
}

static int maxim_read_reg(u8 regoffset, u8 *value)
{
	struct i2c_adapter *adap;
	int ret;
	struct i2c_msg msg[2];

	adap = i2c_get_adapter(MAX17410_QUP_I2C_BUS_ID);
	if (!adap)
		return -ENODEV;

	/* setup the address to read */
	msg[0].addr = MAX17410_SLAVE_ADDR;
	msg[0].len = 1;
	msg[0].flags = 0;
	msg[0].buf = &regoffset;

	/* setup the read buffer */
	msg[1].addr = MAX17410_SLAVE_ADDR;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 2;
	msg[1].buf = value;

	ret = i2c_transfer(adap, msg, 2);

	return (ret >= 0) ? 0 : ret;
}


static int maxim_write_reg(u8 regoffset, u8 msb, u8 lsb)
{
	struct i2c_adapter *adap;
	int ret;
	struct i2c_msg msg;
	u8 buf[4];

	adap = i2c_get_adapter(MAX17410_QUP_I2C_BUS_ID);
	if (!adap){
		MAXIM_FGAUGE_ERR("No Device (MAX17410_QUP_I2C_BUS_ID) \n");
		return -ENODEV;
	}

	buf[0] = regoffset;
	buf[1] = msb;
	buf[2] = lsb;

	/* setup the address to write */
	msg.addr = MAX17410_SLAVE_ADDR;
	msg.flags = 0x0;
	msg.len = 3;
	msg.buf = buf;

	ret = i2c_transfer(adap, &msg, 1);

	return ret;
	
}

static int maxim_write_data(u8 reg_offset, u8 *data, int num_bytes)
{
	struct i2c_adapter *adap;
	int ret;
	struct i2c_msg msg;
	u8 buf[32];

	adap = i2c_get_adapter(MAX17410_QUP_I2C_BUS_ID);
	if (!adap){
		MAXIM_FGAUGE_ERR("No Device (MAX17410_QUP_I2C_BUS_ID) \n");
		return -ENODEV;
	}

	buf[0] = reg_offset;
	memcpy(&buf[1], data, num_bytes);

	/* setup the address to write */
	msg.addr = MAX17410_SLAVE_ADDR;
	msg.flags = 0x0;
	msg.len = num_bytes + 1;
	msg.buf = buf;

	ret = i2c_transfer(adap, &msg, 1);

	return ret;

}


static int max17040_update_rcomp(int batt_temp)
{

	#define INIT_RCOMP 11250  // 0x70*100  + 50 // from  INI file
	#define TEMP_CO_HOT  -30		  // -0.3 *100
	#define TEMP_CO_COLD -520  // -5.2 *100

	int new_rcomp=0;
	//u8 rcomp_buf[2];
	
	if(batt_temp >= 200){
		new_rcomp = INIT_RCOMP + ( ((batt_temp - 200)/10) * TEMP_CO_HOT);
	}

	if(batt_temp < 200){
		new_rcomp = INIT_RCOMP + ( ((batt_temp - 200)/10) * TEMP_CO_COLD);

	}
	new_rcomp /= 100;

	if(new_rcomp > 0xff) new_rcomp = 0xff;
	if(new_rcomp < 0) new_rcomp = 0;

	//printk(KERN_INFO"[gun] new RCOMP = 0x%x\n", new_rcomp);
	m_latest_rcomp_msb = (unsigned char)new_rcomp;

	if( maxim_write_reg(MAX17040_RCOMP_MSB, (u8)(new_rcomp), 0x00) < 0){
		MAXIM_FGAUGE_ERR("max17040 I2C failed to write \n");
		return -1;
	}

	return 0;

}

static int max17040_need_to_update_rcomp(int batt_temp)
{
	#define UPDATE_CONDITION  30   // 3℃
	#define INVALID_BATT_TEMP  -300
		
	static int latest_temp = 200;         // starting temperature
	
	int temp_diff;

	if( batt_temp == INVALID_BATT_TEMP ) return 0;

	temp_diff = batt_temp - latest_temp;

	// check if RCOMP need to be updated		
	if( (temp_diff >= UPDATE_CONDITION) ||(temp_diff <=  -1*UPDATE_CONDITION) )
	{

		printk(KERN_INFO"[gun]::Battery temperature current= %d, latest= %d, diff=%d\n",
			batt_temp, latest_temp, temp_diff);	
		latest_temp = batt_temp;
		return 1;
	}

	return 0;

}


static int max17040_loading_custom_model(void)
{
	u8 rcomp_buf[2];
	u8 ocv_buf[2];
	u8 soc_buf[2];
	u8 data[16];

	printk("::: max17040_loading_custom_model() :::\n");

	// 1. unlock Model Access
	maxim_write_reg(0x3E,0x4A,0x57);

	// 2. Read RCOMP and OCV
	maxim_read_reg(MAX17040_RCOMP_MSB, rcomp_buf);
	printk(KERN_INFO"[gun] RCOMP read value = 0x%x\n", ((rcomp_buf[0]<<8) + rcomp_buf[1]));
	
	maxim_read_reg(MAX17040_OCV_MSB, ocv_buf);
	printk(KERN_INFO"[gun] OCV read value = 0x%x\n", ((ocv_buf[0]<<8) + ocv_buf[1]));

	// 3. Write OCV
	maxim_write_reg(MAX17040_OCV_MSB, 0xE2, 0x00);

	// 4. Write RCOMP to a Maximum value of 0xFF00
	maxim_write_reg(MAX17040_RCOMP_MSB, 0xFF, 0x00);

	// 5. Write Model
	data[0]  = 0x84;
	data[1]  = 0xD0;
	data[2]  = 0xB5;
	data[3]  = 0xD0;
	data[4]  = 0xB7;
	data[5]  = 0x50;
	data[6]  = 0xB9;
	data[7]  = 0x20;
	data[8]  = 0xB9;
	data[9]  = 0x80;
	data[10] = 0xBB;
	data[11] = 0x70;
	data[12] = 0xBC;
	data[13] = 0x80;
	data[14] = 0xBD;
	data[15] = 0x30;
	maxim_write_data(0x40, data, 16);

	data[0]  = 0xBD; 
	data[1]  = 0xC0; 
	data[2]  = 0xBE; 
	data[3]  = 0xE0; 
	data[4]  = 0xC1; 
	data[5]  = 0x90; 
	data[6]  = 0xC5; 
	data[7]  = 0xA0; 
	data[8]  = 0xC8; 
	data[9]  = 0x50; 
	data[10] = 0xCB; 
	data[11] = 0xD0; 
	data[12] = 0xD1; 
	data[13] = 0xC0; 
	data[14] = 0xD8; 
	data[15] = 0x00; 
	maxim_write_data(0x50, data, 16);

	data[0]  = 0x00;
	data[1]  = 0x20;
	data[2]  = 0x21;
	data[3]  = 0x60;
	data[4]  = 0x1A;
	data[5]  = 0x40;
	data[6]  = 0x66;
	data[7]  = 0x10;
	data[8]  = 0x00;
	data[9]  = 0x20;
	data[10] =0x2F;
	data[11] =0x10;
	data[12] =0x4A; 
	data[13] =0xA0; 
	data[14] =0x7A;
	data[15] =0xD0; 
	maxim_write_data(0x60, data, 16);

	data[0]   = 0x2C;
	data[1]   = 0xA0;
	data[2]   = 0x19;
	data[3]   = 0xF0;
	data[4]   = 0x11;
	data[5]   = 0xC0;
	data[6]   = 0x18;
	data[7]   = 0x10;
	data[8]   = 0x12;
	data[9]   = 0xF0;
	data[10] = 0x0D;
	data[11] = 0xF0;
	data[12] = 0x0D;
	data[13] = 0x20;
	data[14] = 0x0D;
	data[15] = 0x20;
	maxim_write_data(0x70, data, 16);

	// 6. Delay at least 150mS
	msleep(150);

	// 7. Write OCV
	maxim_write_reg(MAX17040_OCV_MSB, 0xE2, 0x00);

	// 8. Delay between 150mS and 600mS
	msleep(200);

	// 9. Read SOC Register and Compare to expected result
	maxim_read_reg(MAX17040_SOC_MSB, soc_buf);
	printk(KERN_INFO"[gun] SOC read value = 0x%x\n", ((soc_buf[0]<<8) + soc_buf[1]));

	if((soc_buf[0] < 0xE7) || (soc_buf[0]>0xE9))
	{	
		MAXIM_FGAUGE_ERR("Battery model was not loaded successful soc1 = 0x%x soc2 = 0x%x\n", soc_buf[0], soc_buf[1]);
		return -1;
	} 

	//  10. Restore RCOMP and OCV
	maxim_write_reg(MAX17040_RCOMP_MSB, rcomp_buf[0], rcomp_buf[1]);
	maxim_write_reg(MAX17040_OCV_MSB, ocv_buf[0], ocv_buf[1]);

	//  11. Lock Model Access
	maxim_write_reg(0x3E,0x00,0x00);

	//  12. Update RCOMP
	//maxim_write_reg(MAX17040_RCOMP_MSB, rcomp_buf[0], rcomp_buf[1]);
	MAXIM_FGAUGE_ERR("latest RCOMP: %x\n", m_latest_rcomp_msb);
	maxim_write_reg(MAX17040_RCOMP_MSB, m_latest_rcomp_msb, 0x00);

	msleep(150);
	MAXIM_FGAUGE_ERR("Battery model was loaded successful\n");

	return 0;
	
}



int max17040_get_voltage(void)
{
	static int voltage_err_count = 0;
	u8 voltage[2];
	int vcell;
	int err;
	int i = 0;
	static int max17040_latest_voltage = 0;

#ifdef CONFIG_MACH_KTTECH
	if ( get_kttech_ftm_mode() != KTTECH_FTM_MODE_NONE ){
		// if phone is in FTM mode, return constant SoC
		MAXIM_FGAUGE_ERR("In FTM mode, return a voltage => 3800\n");
		return 3800;
	}
#endif

	for(i = 0; i < MAX17410_I2C_RETRY_NUM; i++)
	{
		err = maxim_read_reg(MAX17040_VCELL_MSB, &voltage[0]);
		if(err >= 0) {
			vcell = (((voltage[0] << 4) + (voltage[1] >> 4)) * 5) >> 2;
			MAXIM_FGAUGE_MSG(" %s: %d %x %x\n", __func__, vcell, voltage[0], voltage[1]);
			max17040_latest_voltage = vcell;
			return vcell;
		}
		voltage_err_count++;
	}	

	MAXIM_FGAUGE_ERR("kttech-charger I2C failed.. %d\n", voltage_err_count);
	return max17040_latest_voltage;
	
}

#if 0
static int max17040_quick_start(void)
{

	MAXIM_FGAUGE_ERR("max17040_quick_start() called \n");

	if(maxim_write_reg(MAX17040_MODE_MSB, 0x40, 0x00) < 0){
		MAXIM_FGAUGE_ERR("MAX17040_MODE_MSB #1 - write failed \n");
		return -1;
	}

	msleep(500);

	if(maxim_write_reg(MAX17040_MODE_MSB, 0x00, 0x00) < 0){
		MAXIM_FGAUGE_ERR("MAX17040_MODE_MSB #2 - write failed \n");
		return -1;
	}
	
	return 0;
	
}
#endif

int max17040_batt_insert_irq_handler(int batt_present)
{

	MAXIM_FGAUGE_ERR("max17040_batt_insert_irq_handler(batt_present= %d)\n", batt_present);

	if(batt_present){

		if(max17040_loading_custom_model() != 0)
		{
			MAXIM_FGAUGE_ERR("failed to load custom model data\n");
			max17040_loading_custom_model();
		}
		//max17040_quick_start();
		//msleep(500);
		m_batt_not_present = 0;
		del_timer(&m_batt_insert_check_timer);
		pm8921_update_battery_status_force();

	}else{
		//max17040_quick_start();
		m_batt_insert_check_timer.expires = jiffies + HZ*3 + HZ/2;
		m_batt_insert_check_timer.data = 0;
		m_batt_insert_check_timer.function = &max17040_batt_insert_check_func;
		printk("[gun] add timer \n");
		add_timer(&m_batt_insert_check_timer);		
	}
	
	return 0;

}


int max17040_get_soc(int batt_temp)
{
	static int soc_err_count = 0;
	u8 soc[2];
	int SOCValue;
	int DisplayedSOC;
	int err;
	int i = 0;

	#ifdef CONFIG_MACH_KTTECH
	if ( get_kttech_ftm_mode() != KTTECH_FTM_MODE_NONE ){
		// if phone is in FTM mode, return constant SoC
		MAXIM_FGAUGE_ERR("In FTM mode, return a constant SoC => 50\n");
		return 50;
	}
	#endif

	if(m_batt_not_present) return 0;

	if(max17040_need_to_update_rcomp(batt_temp)){
		max17040_update_rcomp(batt_temp);
	}

	for(i = 0; i < MAX17410_I2C_RETRY_NUM; i++)
	{
		err = maxim_read_reg(MAX17040_SOC_MSB, &soc[0]);
		if(err >= 0) {
#ifdef KTTECH_MAXIM_BATT_MODEL_18BIT
			//Spider Model : RCOMP = 0xB700, FullSoc = 95.55, EmptySoc = 1.68, 18bit mode
			SOCValue = (( (soc[0] << 8) + soc[1] ) );
			DisplayedSOC = (SOCValue - 431) / 240;
			if(DisplayedSOC > 100)
				DisplayedSOC = 100;
			else if(DisplayedSOC < 0)
				DisplayedSOC = 0;
#else 
			// 19bit mode, FullSoc = 101.80, EmpySoc = 0.68
			// 512 => 1%, FullSocRaw = 512*FullSoc, EmptySocRaw=512*EmptySoc
			// DisplayedSoc = (SocRaw - EmptySocRaw)/((FullSocRaw-EmptySocRaw)/100)
			//                           = (SocRaw - 512*0.68)/((512*101.8 - 512*0.68)/100)
			//                           =(SocRaw - 348)/((52121-348)/100)

			// 19bit mode, FullSoc = 98.0 , EmpySoc = 0.2
			// 512 => 1%, FullSocRaw = 512*FullSoc, EmptySocRaw=512*EmptySoc
			// DisplayedSoc = (SocRaw - EmptySocRaw)/((FullSocRaw-EmptySocRaw)/100)
			//                           = (SocRaw - 512*0.2)/((512*98.2 - 512*0.2)/100)
			//                           =(SocRaw - 102)/(50176-102)/100)
			SOCValue = (( (soc[0] << 8) + soc[1] ) );

			// additional compensation(or trick) for low battery state
			// The voltage level of SoC 1% should be changed to 3.55V
			if(SOCValue < 25000){
				if(likely(SOCValue > 350)){
					SOCValue += (35000/SOCValue);
				}
			}
			
			DisplayedSOC = (SOCValue - 0) / 496;
			if(DisplayedSOC > 100)
				DisplayedSOC = 100;
			else if(DisplayedSOC < 0)
				DisplayedSOC = 0;
#endif
			//MAXIM_FGAUGE_ERR("Maxim17040:: displayedSoC=%d RawSoC=0x%x%02x Batt_temp: %d\n", 
				//DisplayedSOC, soc[0], soc[1], batt_temp);
			return DisplayedSOC;
		}
		soc_err_count++;
	}

	MAXIM_FGAUGE_ERR("kttech-charger I2C failed. %d\n", soc_err_count);
	return err;
}

int max17040_get_temperature(void)
{
	return 30;
}

static int __init max17040_init(void)
{
	printk("[gun]max17040_init() called\n");
	init_timer(&m_batt_insert_check_timer);
	return 0;
}
module_init(max17040_init);

static void __exit max17040_exit(void)
{
	del_timer_sync(&m_batt_insert_check_timer);
}
module_exit(max17040_exit);

MODULE_DESCRIPTION("KTtech MAX17040 Fuel Gauge");
MODULE_LICENSE("GPL");

#endif
