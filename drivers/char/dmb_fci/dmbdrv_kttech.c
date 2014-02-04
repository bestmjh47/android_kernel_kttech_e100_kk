/*******************************************************************************************
File: dmbdrv.c
Description: DMB Device Driver
Writer: KTTECH SW1 khkim
LastUpdate: 2010-06-30
*******************************************************************************************/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/jiffies.h>
#include <asm/uaccess.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include "dmbdrv_kttech.h"
#include <asm/io.h>
#include <linux/regulator/consumer.h>
#include <linux/spi/spi.h>
#include <linux/workqueue.h>
#include <linux/mutex.h>

#include "fic.h"
#include "bbm.h"
#include "fci_oal.h"
#include "fci_tun.h"
#include "fc8050_regs.h"
#include <linux/kthread.h>
#include <linux/irq.h>
#include <linux/interrupt.h>

#include <linux/wait.h>
#include <linux/kttgenmod.h>

// kuzuri 2012.01.20 - MSM internal XO_A1 use
#define DMB_USE_MSM_XO

#ifdef DMB_USE_MSM_XO
#include <mach/msm_xo.h>
#endif

#define USE_FCI_STATIC_MEM

#define DMB_USE_POLL
#ifdef DMB_USE_POLL
#include <linux/poll.h>
#endif

#include "fci_msg.h"

#define TDMB_SPI_INTERFACE
//#define TDMB_SPI_INTERFACE_TEST

#define DMB_MAJOR 260

#define DMB_GPIO_DEMOD_EN_N                90
#define DMB_GPIO_RESET_N                         77
#define DMB_GPIO_INT_N                              78

#define TDMB_DEV_NAME       "tdmb"
#define TDMB_DRV_NAME       "dmbdrv"
#define TDMB_CLASS_NAME   "dmbclass"

#define KX_DMB_LNA_GAIN 0
#define KXDMB_RSSI_LEVEL_0_MAX	(85)
#define KXDMB_RSSI_LEVEL_1_MAX	(78)
#define KXDMB_RSSI_LEVEL_2_MAX	(74)
#define KXDMB_RSSI_LEVEL_3_MAX	(71)
#define KXDMB_RSSI_LEVEL_4_MAX	(67)
#define KXDMB_RSSI_LEVEL_5_MAX	(63)
#define KXDMB_RSSI_LEVEL_6_MAX	(59)

#define	ABS(a)			(((a) < 0)?-(a):(a))

typedef enum
{
	KXDMB_RSSI_LEVEL_0,	// - (94) dBm
	KXDMB_RSSI_LEVEL_1,	// - (86) dBm
	KXDMB_RSSI_LEVEL_2,	// - (78) dBm
	KXDMB_RSSI_LEVEL_3,	// - (70) dBm
	KXDMB_RSSI_LEVEL_4,	// - (62) dBm
	KXDMB_RSSI_LEVEL_5,	// - (54) dBm
	KXDMB_RSSI_LEVEL_6,	// - (46) dBm
	KXDMB_RSSI_LEVEL_MAX,
} KTFT_TDMB_RSSI_LEVEL_T; 

static ssize_t dmbdev_read (struct file *file, char __user *buf, size_t count, loff_t *offset);
static ssize_t dmbdev_write (struct file *file, const char __user *buf, size_t count, loff_t *offset);
#ifdef DMB_USE_POLL
static unsigned int dmbdev_poll( struct file *filp, poll_table *wait );
#endif
static long dmbdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg);
static int dmbdev_open(struct inode *inode, struct file *file);
static int dmbdev_release(struct inode *inode, struct file *file);
static int set_ch_data(DMB_CH_DATA_S* pchbuf, unsigned int freq);
static int get_rssi_level(unsigned int rssi);
static void tdmb_gpios_free(const struct msm_gpio *table, int size);
static int tdmb_gpios_disable(const struct msm_gpio *table, int size);
static int tdmb_power(int on);
//int fic_callback(HANDLE hDevice, u8 *data, int len);
int fic_callback(void *hDevice, unsigned char *data, int len);
//int msc_callback(HANDLE hDevice, u8 subChId, u8 *data, int len);
int msc_callback(void *hDevice, unsigned char subChId, unsigned char *data, int len);

static struct class *dmbclass;
static struct device *dmbdev;

#ifdef TDMB_SPI_INTERFACE
extern struct spi_device *fc8050_spi;
#endif

static char start_service = 0;
//wait_queue_head_t g_RingBuffer_wq;

//#define USE_STREAM_BUFF

#ifdef USE_STREAM_BUFF
static unsigned char *g_StreamBuff;
#endif

#ifdef DMB_USE_POLL
#define INT_OCCUR_SIG		0x0A		// [S5PV210_Kernel], 20101220, ASJ, 
#define DIRECT_OUT_SIG		0x01
wait_queue_head_t WaitQueue_Read;
static unsigned char ReadQ;
#endif

static int b_First_call_poll = 0;

//static int test_overrun = 0;


//DMB_OPEN_INFO_T *g_hOpen = NULL;


#ifdef DMB_USE_MSM_XO
static struct msm_xo_voter *dmb_clock;
#endif

struct completion fci_comp;

static struct msm_gpio tdmb_gpio_config_data[] = {
	{ GPIO_CFG(DMB_GPIO_INT_N, 0, GPIO_CFG_INPUT, GPIO_CFG_NO_PULL, GPIO_CFG_2MA), "tdmb_irq" },
	{ GPIO_CFG(DMB_GPIO_RESET_N, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "tdmb_reset" },
	{ GPIO_CFG(DMB_GPIO_DEMOD_EN_N, 0, GPIO_CFG_OUTPUT, GPIO_CFG_PULL_UP, GPIO_CFG_2MA), "tdmb_dem_enable" },
};


int tdmb_gpios_enable(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_ENABLE);
		if (rc) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_ENABLE)"	" <%s> failed: %d\n", g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n", GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg), GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg),	GPIO_DRVSTR(g->gpio_cfg));
			goto err;
		}
	}
	return 0;
	
	err:
		tdmb_gpios_disable(table, i);
	return rc;
}

int tdmb_gpios_disable(const struct msm_gpio *table, int size)
{
	int rc = 0;
	int i;
	const struct msm_gpio *g;
	
	for (i = size-1; i >= 0; i--) {
		int tmp;
		g = table + i;
		tmp = gpio_tlmm_config(g->gpio_cfg, GPIO_CFG_DISABLE);
		if (tmp) {
			pr_err("gpio_tlmm_config(0x%08x, GPIO_CFG_DISABLE)" " <%s> failed: %d\n",	g->gpio_cfg, g->label ?: "?", rc);
			pr_err("pin %d func %d dir %d pull %d drvstr %d\n", GPIO_PIN(g->gpio_cfg), GPIO_FUNC(g->gpio_cfg), GPIO_DIR(g->gpio_cfg), GPIO_PULL(g->gpio_cfg), GPIO_DRVSTR(g->gpio_cfg));
			if (!rc)
				rc = tmp;
		}
	}
	return rc;
}

int tdmb_gpios_request(const struct msm_gpio *table, int size)
{
	int rc;
	int i;
	const struct msm_gpio *g;
	
	for (i = 0; i < size; i++) {
		g = table + i;
		rc = gpio_request(GPIO_PIN(g->gpio_cfg), g->label);
		DEVLOG("[%s]:: gpio_request(%d : %s)--> %d\n", __func__, GPIO_PIN(g->gpio_cfg), g->label, rc);
		if (rc) {
			pr_err("gpio_request(%d) <%s> failed: %d\n", GPIO_PIN(g->gpio_cfg), g->label ?: "?", rc);
			goto err;
		}
		if ( GPIO_PIN(g->gpio_cfg)==DMB_GPIO_INT_N )
		{
			rc = gpio_direction_output(GPIO_PIN(g->gpio_cfg), 1);
			if ( rc )
				pr_err("gpio_request:: IRQ output -> HIGH... fail...: %d\n", GPIO_PIN(g->gpio_cfg) );
			else
				pr_err("gpio_request:: IRQ output -> HIGH... SUCCESS...: %d\n", GPIO_PIN(g->gpio_cfg) );
		}
	}
	
	return 0;
	
	err:
		tdmb_gpios_free(table, i);
	return rc;
}

void tdmb_gpios_free(const struct msm_gpio *table, int size)
{
	int i;
	const struct msm_gpio *g;
	
	for (i = size-1; i >= 0; i--) {
		g = table + i;
		gpio_free(GPIO_PIN(g->gpio_cfg));
	}
}

int tdmb_gpios_request_enable(const struct msm_gpio *table, int size)
{
	int rc = tdmb_gpios_request(table, size);
	
	if (rc)
		return rc;
	
	rc = tdmb_gpios_enable(table, size);
	
	if (rc)
		tdmb_gpios_free(table, size);
	
	return rc;
}

void tdmb_gpios_disable_free(const struct msm_gpio *table, int size)
{
	tdmb_gpios_disable(table, size);
	tdmb_gpios_free(table, size);
}

static int tdmb_gpio_setup(void)
{
	int rc;

//	rc = gpio_request(DMB_GPIO_INT_N, "ts_int");
//		DEVLOG(" [%s]: fail gpio_req():: IRQ fail = %d\n", rc);

	rc = tdmb_gpios_request_enable(tdmb_gpio_config_data,
	ARRAY_SIZE(tdmb_gpio_config_data));

	return rc;
}

static void tdmb_gpio_teardown(void)
{
	tdmb_gpios_disable_free(tdmb_gpio_config_data, ARRAY_SIZE(tdmb_gpio_config_data));
}

static struct tdmb_platform_data tdmb_pdata = {
	.power  = tdmb_power,
	.setup    = tdmb_gpio_setup,
	.teardown = tdmb_gpio_teardown,
#ifdef TDMB_SPI_INTERFACE
	.irq = MSM_GPIO_TO_INT(DMB_GPIO_INT_N),
#else
	.irq = 0,
#endif	
	.reset = DMB_GPIO_RESET_N,
	.demod_enable = DMB_GPIO_DEMOD_EN_N,
};

static void tdmb_reset(int enable)
{
	if(enable)
	{
		//2011.11.28 vincent
		gpio_set_value(tdmb_pdata.reset, 1);
		msleep(30);
		gpio_set_value(tdmb_pdata.reset, 0);
		msleep(30);  // 2011.06.28
		gpio_set_value(tdmb_pdata.reset, 1);
		DEVLOG(" reset fc8050 chip \n");
	}
	else
		gpio_set_value(tdmb_pdata.reset, 0);
}

static void tdmb_demod_enable(int enable)
{
	gpio_set_value(tdmb_pdata.demod_enable, enable);
	
	//2011.11.28 vincent
	msleep(50);
}

//static struct regulator *dmb_8058_l10; // 1.8V Main // spider
static struct regulator *dmb_main_power; // 1.8V Main // spider

static int tdmb_power(int on)
{
  int rc = 0;

  // spider_current ---> start

  //DEVLOG("DMB::  power (%d) _________: spi= %s \n",on, dmbdev->driver->name);
  DEVLOG("DMB::  power (%d) _________:\n",on);

  if(on)
  {
    //dmb_8058_l10 = regulator_get(NULL, "8058_l10");
    dmb_main_power = regulator_get(&fc8050_spi->dev, "tdmb_vol");
    if (IS_ERR(dmb_main_power))
    {
      pr_err("%s: regulator_get(8921_l29) failed (%ld)\n",
      __func__, PTR_ERR(dmb_main_power));
      return PTR_ERR(dmb_main_power);
    }

    //set voltage level
    rc = regulator_set_voltage(dmb_main_power, 1800000, 1800000);

    if (rc)
    { 
      pr_err("%s: Spider regulator_set_voltage(8921_l29) failed (%d)\n",
      __func__, rc);
      regulator_put(dmb_main_power);
      return rc;
    }

    // L29 0.4 voltage dropping fix
    rc = regulator_set_optimum_mode(dmb_main_power, 200000);
    if (rc < 0) {
        pr_err("%s: set_optimum_mode(8921_l29) failed, rc=%d\n", __func__, rc);
        regulator_put(dmb_main_power);
        return rc;
    }

    //enable output
    rc = regulator_enable(dmb_main_power);
    if (rc)
    { 
      pr_err("%s: regulator_enable(8921_l29) failed (%d)\n", __func__, rc);
      regulator_put(dmb_main_power);
      return rc;
    }
    msleep(2);  // 2011.06.28
    tdmb_demod_enable(on);

    //msleep(50); // kuzuri 11.28 test xo 

    #ifdef DMB_USE_MSM_XO
    dmb_clock = msm_xo_get(MSM_XO_TCXO_A1, "dmb_clock");
    if (IS_ERR(dmb_clock)) {
		rc = PTR_ERR(dmb_clock);
		printk(KERN_ERR "%s: Couldn't get TCXO_A1 vote for DMB (%d)\n",
					__func__, rc);
	}

	rc = msm_xo_mode_vote(dmb_clock, MSM_XO_MODE_ON);
	if (rc < 0) {
		printk(KERN_ERR "%s:  Failed to vote for TCX0_A1 ON (%d)\n",
					__func__, rc);
	}
	#endif
    
    msleep(1);  // 2011.06.28
    tdmb_reset(on);
  }
  else
  {
    #if 1
    if (!dmb_main_power)
      return rc;
    #else
    if (!dmb_8058_l10)
    {
      dmb_8058_l10 = regulator_get(NULL, "8058_l10");
      if (IS_ERR(dmb_8058_l10))
      {
        pr_err("%s: regulator_get(8058_l10) failed (%d)\n",
        __func__, rc);
        return PTR_ERR(dmb_8058_l10);
      }
    }
    #endif

    rc = regulator_disable(dmb_main_power);

    if (rc)
    { 
      pr_err("%s: regulator_disable(8921_l29) failed (%d)\n",
      __func__, rc);
      regulator_put(dmb_main_power);
      return rc;
    }
    regulator_put(dmb_main_power);
    dmb_main_power = NULL;
    tdmb_reset(on);
    tdmb_demod_enable(on);

  #ifdef DMB_USE_MSM_XO
    if (dmb_clock != NULL) {
		rc = msm_xo_mode_vote(dmb_clock, MSM_XO_MODE_OFF);
		if (rc < 0) {
			printk(KERN_ERR "%s: Voting off DMB XO clock (%d)\n",
					__func__, rc);
		}
		msm_xo_put(dmb_clock);
	}
	#endif

	gpio_set_value_cansleep(tdmb_pdata.reset, 0);
  	gpio_set_value_cansleep(tdmb_pdata.demod_enable, 0);
  	gpio_set_value_cansleep(DMB_GPIO_INT_N, 0);
    
  }

  //msleep(10);
  return 0;

  // <<-------- // spider_current
}

u32 TDMB_GET_KOREABAND_FULL_TABLE(u16 uiIndex)
{
	if(uiIndex >= MAX_KOREABAND_FULL_CHANNEL) return 0xFFFF;
	return g_uiKOREnsembleFullFreq[uiIndex];
}

#define RING_BUFFER_SIZE	(128 * 1024)  // kmalloc max 128k
static DECLARE_WAIT_QUEUE_HEAD(dmb_isr_wait);
static u8 dmb_isr_sig=0;
static struct task_struct *dmb_kthread = NULL;

static u8 dmb_preber_ready_isr_lock = 0;

static int b_exit_dmb_thread = 0;

SubChInfoTypeDB gDMBSubChInfo;
SubChInfoTypeDB gDABSubChInfo;

static u32 gInitFlag = 0;
DMB_INIT_INFO_T *hInit;

#ifdef USE_FCI_STATIC_MEM
DMB_INIT_INFO_T m_hInit;
DMB_OPEN_INFO_T m_hOpen;
ensemble_scan_req_t m_pscan;
u8 hopen_buf[RING_BUFFER_SIZE];
#endif


static int dmb_thread(void *hDevice)
{
  static DEFINE_MUTEX(thread_lock);

  DMB_INIT_INFO_T *hInit = (DMB_INIT_INFO_T *)hDevice;

  set_user_nice(current, -20);

  PRINTF(hInit, "dmb_kthread enter\n");

  BBM_FIC_CALLBACK_REGISTER((u32)hInit, fic_callback);
  BBM_MSC_CALLBACK_REGISTER((u32)hInit, msc_callback);

  while(1)
  {	  
    wait_event_interruptible(dmb_isr_wait, dmb_isr_sig || kthread_should_stop());
    dmb_isr_sig=0;
    dmb_preber_ready_isr_lock = 1;
    BBM_ISR(hInit);
    dmb_preber_ready_isr_lock = 0;

    if ( b_exit_dmb_thread==1 )
    {
      DEVLOG("DMB_Thread... loop.. exit dmb thread = 1 \n");
      break;
    }

#ifdef DMB_USE_POLL
    ReadQ = INT_OCCUR_SIG;
    wake_up_interruptible(&WaitQueue_Read);
#endif

    if (kthread_should_stop())
      break;

  }

  // 2011.06.28
  BBM_FIC_CALLBACK_DEREGISTER(NULL);
  BBM_MSC_CALLBACK_DEREGISTER(NULL);

  PRINTF(hInit, "dmb_kthread exit\n");
  dmb_kthread = NULL;

  return 0;
}


int DMBDrv_init(void)
{
  s32 ret;
  DEVLOG(" DMB DRV INIT()_____:\n");

#ifndef USE_FCI_STATIC_MEM
  if ( !hInit )
    hInit = (DMB_INIT_INFO_T *)kmalloc(sizeof(DMB_INIT_INFO_T), GFP_KERNEL);
  DEVLOG(" MALLOC()___ hInit addr= %X  ,size= %d\n", hInit, sizeof(DMB_INIT_INFO_T));
#else
  hInit = &m_hInit;
//  DEVLOG(" STATIC_LOC()___ hInit addr= %X  ,size= %d\n", hInit, sizeof(DMB_INIT_INFO_T));
#endif

  ret = BBM_HOSTIF_SELECT(hInit, BBM_SPI);

  if(ret)
    PRINTF(hInit, "dmb host interface select fail!\n");

#if 0
  if (!dmb_kthread)
  {
    dmb_kthread = kthread_run(dmb_thread, (void*)hInit, "dmb_thread");
  }
#endif

  INIT_LIST_HEAD(&(hInit->hHead));

  PRINTF(hInit, "dmb init \n");
  return 0;
}

void DMBDrv_exit(void)
{
  PRINTF(hInit, "dmb exit \n");

  if ( tdmb_pdata.irq )
  {
    DEVLOG("DMB: Free_IRQ 1-------->>>>\n");
    free_irq(tdmb_pdata.irq, NULL);
    //tdmb_pdata.irq = NULL;
  }

  #if 0
  if ( dmb_kthread )
  {
    kthread_stop(dmb_kthread);
    dmb_kthread=NULL;
  }
  #endif

  BBM_HOSTIF_DESELECT(hInit);

  DEVLOG(" hInit FREE ++++++\n");
  #ifndef USE_FCI_STATIC_MEM
  kfree(hInit);  
  hInit = NULL;
  #endif
}

unsigned char DMBDrv_SVC_Stop(void)
{
	BBM_VIDEO_DESELECT(NULL, 0, 0, 0);
	BBM_AUDIO_DESELECT(NULL, 0, 3);
	BBM_DATA_DESELECT(NULL, 0, 2);

	//BBM_WRITE(NULL, BBM_COM_INT_ENABLE, 0x00); // 2011.06.28 // kuzuri.11.25
	
	return BBM_OK;
}

unsigned char DMBDrv_SetCh(unsigned long ulFrequency, unsigned char ucSubChannel, unsigned char ucSvType)
{
  int ret;

  DEVLOG("DMBDRv Set ch():: SCAN freq(%ld) is Start... initflag= %d\n", ulFrequency, gInitFlag);

  ret = 0;

  if(!gInitFlag)
    return -2;

  BBM_WORD_WRITE(NULL, BBM_BUF_INT, 0x00ff);      // disable FIC int

  BBM_VIDEO_DESELECT(NULL, 0, 0, 0);
  BBM_AUDIO_DESELECT(NULL, 0, 3);
  BBM_DATA_DESELECT(NULL, 0, 2);


 //2011.10.26 - FCI
 // for ( i=0; i<3; i++ )
 // {
    if(BBM_TUNER_SET_FREQ(NULL, ulFrequency) != BBM_OK)
    {
      return -1;
    }

  if(BBM_SCAN_STATUS(NULL)) {
	printk("DMBDRv Set ch():: BBM SCAN STATUS is fail... \n");
	return BBM_NOK;
  }

  msleep(100); //kuzur_test 11.30

  DEVLOG("DMB:: Set Ch()-------   ucSvType = %X  , ucSubCh= %d , BUF_CH3_END= %d\n", ucSvType, ucSubChannel, CH3_BUF_END );

  if(ucSvType == 0x18) 
  {
  BBM_VIDEO_SELECT(NULL, ucSubChannel, 0, 0);
  }	
  else if(ucSvType == 0x00) 
  {
    #if 0
    // __________set speed... 2011.07.05 DAB 
   	BBM_WORD_WRITE(NULL, BBM_BUF_CH3_END, 	CH3_BUF_END );  //(CH3_BUF_START + 160*3*2 - 1)); // 160 kbps set.
	BBM_WORD_WRITE(NULL, BBM_BUF_CH3_THR, 	CH3_BUF_THR ); //(160*3-1));  // 160 kbps set
	#endif
    
    BBM_AUDIO_SELECT(NULL, ucSubChannel, 3);
  }
  else
  {
    // __________ set speed...
    BBM_DATA_SELECT(NULL, ucSubChannel, 2);
  }

  //msleep(100); //kuzur_test 11.30

  //BBM_WRITE(NULL, BBM_COM_INT_ENABLE, 0x1); // 2011.06.28_temp // kuzuri.11.25

  return BBM_OK;
}

void fc8050_overrun_check(void)// 2011.11.15 FCI Vincent
{
   u16 overrun = 0;
   u16 buf_enable=0;   
   u8  extIntStatus = 0;
   int ret_val = 0;
   
   ret_val = BBM_WORD_READ(NULL, BBM_BUF_OVERRUN, &overrun);

   if(((ret_val!=0) || (overrun&0x01)))
   {
      DMB_OPEN_INFO_T *hOpen;
      hOpen = &m_hOpen;
      hOpen->buf = hopen_buf;
      
    printk(KERN_ERR "\n\n*** In preber___> Overrun occured !!!!!!!!!!!!!!!!!!!  __ ret= %d , overrun= %d \n\n\n", ret_val, overrun);	
	 BBM_WORD_WRITE(NULL, BBM_BUF_OVERRUN, overrun);
	 BBM_WORD_WRITE(NULL, BBM_BUF_OVERRUN, 0x0000);

	 //buffer restore
	 BBM_WORD_READ(NULL, BBM_BUF_ENABLE, &buf_enable);
	 buf_enable &= ~overrun;
	 BBM_WORD_WRITE(NULL, BBM_BUF_ENABLE, buf_enable);
	 buf_enable |= overrun; 
	 BBM_WORD_WRITE(NULL, BBM_BUF_ENABLE, buf_enable);

	 //external interrupt restore	   
	 BBM_READ(NULL, BBM_COM_INT_STATUS, &extIntStatus);
	 BBM_WRITE(NULL, BBM_COM_INT_STATUS, extIntStatus);
	 BBM_WRITE(NULL, BBM_COM_INT_STATUS, 0x00);

	 fci_ringbuffer_flush(&hOpen->RingBuffer);
	}
}

static s32 DMBDrv_FC8050_Get_Viterbi_RT_Ber(u32* ber)
{
	u32 vframe, esum;
	u8  vt_ctrl=0;
	u32 lber;

	BBM_READ(NULL, BBM_VT_CONTROL, &vt_ctrl);
	vt_ctrl |= 0x10;
	BBM_WRITE(NULL, BBM_VT_CONTROL, vt_ctrl);
	
	BBM_LONG_READ(NULL,BBM_VT_RT_BER_PERIOD, &vframe);
	BBM_LONG_READ(NULL,BBM_VT_RT_ERROR_SUM, &esum);
	
	vt_ctrl &= ~0x10;
	BBM_WRITE(NULL,BBM_VT_CONTROL, vt_ctrl);
	
	if(vframe == 0) {
		lber = 2000;
		return BBM_NOK;
	}

	if(esum > 429496)
		lber = ((esum * 100)/vframe)*100;
	else
		lber = (esum * 10000) / vframe;
	
	*ber = lber;

	//2011.11.28 vincent
    fc8050_overrun_check();    	
	
	return BBM_OK;
}

static s32 DMBDrv_FC8050_Get_RS_Ber(u32* ber)
{
	u32 bper, esum;
	u16 nframe, rserror;

	BBM_WORD_READ(NULL, BBM_RS_BER_PERIOD, &nframe);
	BBM_LONG_READ(NULL, BBM_RS_ERR_SUM, &esum);
	BBM_WORD_READ(NULL, BBM_RS_FAIL_COUNT, &rserror);

	if(nframe == 0) {
		bper = 2000;
		return BBM_NOK;
	}

	bper = esum;
	bper += rserror * 9 * 4;
	
	if(esum > 42926)
		bper = ((bper * 1000) / ((nframe + 1) * 204 * 8))*100;
	else
		bper = (bper * 100000) / ((nframe + 1) * 204 * 8);

	*ber = bper;
	
	return BBM_OK;
}

unsigned char DMBDrv_FC8050_Get_RSSI(s32* rssi)
{
	static s32 pre_rssi = -110;

	*rssi = pre_rssi;
	
	if(!gInitFlag)
		return BBM_NOK;
	
	if(BBM_TUNER_GET_RSSI(NULL, rssi))
		return BBM_NOK;

	pre_rssi = *rssi;

	*rssi = ABS(*rssi);
	return BBM_OK;	
}

unsigned char DMBDrv_ScanCh(unsigned long ulFrequency)
{
	esbInfo_t* esb;

	if(!gInitFlag)
		return BBM_NOK;

	FIC_DEC_SubChInfoClean();

//2011.10.26 - FCI
//  	BBM_WRITE(NULL, BBM_COM_INT_ENABLE, 0); // kuzuri.11.25

//2012.07.06
//  주파수 설정 후 인터럽트 enable 하도록 수정 
//	BBM_WORD_WRITE(NULL, BBM_BUF_INT, 0x01ff); 

	if(BBM_TUNER_SET_FREQ(NULL, ulFrequency)) {
		BBM_WORD_WRITE(NULL, BBM_BUF_INT, 0x00ff);

//2011.10.26 - FCI
//    	BBM_WRITE(NULL, BBM_COM_INT_ENABLE, BBM_MF_INT);
		return BBM_NOK;
	} 

//2012.07.06
//BBM_TUNER_SET_FREQ 호출 전에 인터럽트를 enable하면 간헐적으로 이전 주파수 결과가 인터럽트로 발생하여 
//주파수 설정 후 인터럽트 enable 하도록 수정 
	BBM_WORD_WRITE(NULL, BBM_BUF_INT, 0x01ff); 

	if(BBM_SCAN_STATUS(NULL)) {
		BBM_WORD_WRITE(NULL, BBM_BUF_INT, 0x00ff);
//2011.10.26 - FCI
//        BBM_WRITE(NULL, BBM_COM_INT_ENABLE, BBM_MF_INT);
		return BBM_NOK;
	}
  
//2011.10.26 - FCI
//    BBM_WRITE(NULL, BBM_COM_INT_ENABLE, BBM_MF_INT);

	// wait 1.2 sec for gathering fic information
	msWait(1200);   // 1200
	
	BBM_WORD_WRITE(NULL, BBM_BUF_INT, 0x00ff);

	esb = FIC_DEC_GetEsbInfo(0);
	if(esb->flag != 99) {
		FIC_DEC_SubChInfoClean();
		return BBM_NOK;
	}

	if(strlen((char *)esb->label) <= 0) {
		FIC_DEC_SubChInfoClean();
		return BBM_NOK;
	}
		
	return BBM_OK;
}

static int DMBDrv_GetDMBSubChCnt(void)
{
	svcInfo_t *pSvcInfo;
	int i,n;

	if(!gInitFlag)
		return 0;

	n = 0;
	for(i=0; i <MAX_SVC_NUM; i++) {
		pSvcInfo = FIC_DEC_GetSvcInfoList(i);

		if((pSvcInfo->flag &0x07) == 0x07) {
			if((pSvcInfo->TMId == 0x01) && (pSvcInfo->DSCTy == 0x18))	
				n++;
		}
	}

	return n;
}

static int DMBDrv_GetDABSubChCnt(void)
{
	svcInfo_t *pSvcInfo;
	int i, n;

	if(!gInitFlag)
		return 0;

	n = 0;
	for(i=0; i < MAX_SVC_NUM; i++) {
		pSvcInfo = FIC_DEC_GetSvcInfoList(i);

		if((pSvcInfo->flag &0x07) == 0x07) {
			if((pSvcInfo->TMId == 0x00) && (pSvcInfo->ASCTy == 0x00))	
				n++;
		}
	}

	return n;
}

char* DMBDrv_GetEnsembleLabel(unsigned short* eid)
{
	esbInfo_t* esb;
	
	if(!gInitFlag)
		return NULL;
	
	esb = FIC_DEC_GetEsbInfo(0);

	if(esb->flag == 99)
	{
		*eid = esb->EId;
		return (char*)esb->label;
	}

	return NULL;
}

char* DMBDrv_GetSubChDMBLabel(int nSubChCnt)
{
	int i,n;
	svcInfo_t *pSvcInfo;
	char* label = NULL;

	if(!gInitFlag)
		return NULL;

	n = 0;
	for(i=0; i < MAX_SVC_NUM; i++) {
		pSvcInfo = FIC_DEC_GetSvcInfoList(i);

		if((pSvcInfo->flag & 0x07) == 0x07) {
			if((pSvcInfo->TMId == 0x01) && (pSvcInfo->DSCTy == 0x18)) {
				if(n == nSubChCnt) {
					label = (char*) pSvcInfo->label;
					break;
				}
				n++;
			}
		}
	}

	return label;
}

char* DMBDrv_GetSubChDABLabel(int nSubChCnt)
{
	int i, n;
	svcInfo_t *pSvcInfo;
	char* label = NULL;

	if(!gInitFlag)
		return NULL;

	n = 0;
	for(i=0; i < MAX_SVC_NUM; i++) {
		pSvcInfo = FIC_DEC_GetSvcInfoList(i);

		if((pSvcInfo->flag &0x07) == 0x07) {
			if((pSvcInfo->TMId == 0x00) && (pSvcInfo->ASCTy == 0x00)) {
				if(n == nSubChCnt) {
					label = (char*) pSvcInfo->label;
					break;
				}
				n++;
			}
		}
	}

	return label;
}

SubChInfoTypeDB* DMBDrv_GetFICDMB(int nSubChCnt)
{
	int i, n, j;
	esbInfo_t* esb;
	svcInfo_t *pSvcInfo;
	u8 NumberofUserAppl;

	if(!gInitFlag)
		return NULL;

	memset((void*)&gDMBSubChInfo, 0, sizeof(gDMBSubChInfo));

	n = 0;
	for(i=0; i < MAX_SVC_NUM; i++) {
		pSvcInfo = FIC_DEC_GetSvcInfoList(i);

		if((pSvcInfo->flag &0x07) == 0x07) {
			if((pSvcInfo->TMId == 0x01) && (pSvcInfo->DSCTy == 0x18)) {
				if(n == nSubChCnt) {
					gDMBSubChInfo.ucSubchID         = pSvcInfo->SubChId;
					gDMBSubChInfo.uiStartAddress    = 0;
					gDMBSubChInfo.ucTMId            = pSvcInfo->TMId;
					gDMBSubChInfo.ucServiceType     = pSvcInfo->DSCTy;
					gDMBSubChInfo.ulServiceID       = pSvcInfo->SId;

					NumberofUserAppl = pSvcInfo->NumberofUserAppl;
					gDMBSubChInfo.NumberofUserAppl  = NumberofUserAppl;
					for(j = 0; j < NumberofUserAppl; j++) {
						gDMBSubChInfo.UserApplType[j] = pSvcInfo->UserApplType[j];
						gDMBSubChInfo.UserApplLength[j] = pSvcInfo->UserApplLength[j];
						memcpy(&gDMBSubChInfo.UserApplData[j][0], &pSvcInfo->UserApplData[j][0], gDMBSubChInfo.UserApplLength[j]);
					}

					esb = FIC_DEC_GetEsbInfo(0);
					if(esb->flag == 99) 
						gDMBSubChInfo.uiEnsembleID = esb->EId;
					else                
						gDMBSubChInfo.uiEnsembleID = 0;
					
					break;
				}
				n++;
			}
		}
	}

	return &gDMBSubChInfo;
}

SubChInfoTypeDB* DMBDrv_GetFICDAB(int nSubChCnt)
{
	int i,n;
	esbInfo_t* esb;
	svcInfo_t *pSvcInfo;

	if(!gInitFlag)
		return NULL;
	
	memset((void*)&gDABSubChInfo, 0, sizeof(gDABSubChInfo));

	n = 0;
	for(i=0; i < MAX_SVC_NUM; i++) {
		pSvcInfo = FIC_DEC_GetSvcInfoList(i);

		if((pSvcInfo->flag &0x07) == 0x07) {
			if((pSvcInfo->TMId == 0x00) && (pSvcInfo->ASCTy == 0x00)) {
				if(n == nSubChCnt) {
					gDABSubChInfo.ucSubchID         = pSvcInfo->SubChId;
					gDABSubChInfo.uiStartAddress    = 0;
					gDABSubChInfo.ucTMId            = pSvcInfo->TMId;
					gDABSubChInfo.ucServiceType     = pSvcInfo->ASCTy;
					gDABSubChInfo.ulServiceID       = pSvcInfo->SId;
					esb = FIC_DEC_GetEsbInfo(0);
					if(esb->flag == 99) 
						gDMBSubChInfo.uiEnsembleID = esb->EId;
					else                
						gDMBSubChInfo.uiEnsembleID = 0;

					break;
				}
				n++;
			}
		}
	}

	return &gDABSubChInfo;
}

int DMBDrv_GetSubchBitRate(int subChId) 
{
	// Sub-channel speed
	didpInfo_t  didp;
	subChInfo_t *pSubChInfo;

	pSubChInfo = FIC_DEC_GetSubChInfo(subChId);
	if(pSubChInfo == NULL) {
		PRINTF(hInit, "There is not a subch-info.\n");
		return 0;
	}

	if(pSubChInfo->flag != 99) {
		PRINTF(hInit, "A subch-info was not completed.\n");
		return 0;
	}

	FIC_DEC_SubChOrgan2DidpReg(pSubChInfo, &didp);
	return didp.speed;
}

static irqreturn_t dmb_irq(int irq, void *dev_id)
{
  if (start_service==0 )
  {
    DEVLOG("DMB_IRQ::::  start service is NOT !_____\n");
  }

  //DEVLOG("DMB_IRQ::::  __________++++++++++++++\n");

  //if ( dmb_thread==NULL )
  //  DEVLOG("DMB_IRQ:: ########  Thread is NULL ... error\n");

  dmb_isr_sig=1;
  wake_up_interruptible(&dmb_isr_wait);

  return IRQ_HANDLED;
}

//int fic_callback(HANDLE hDevice, u8 *data, int len)
int fic_callback(void *hDevice, unsigned char *data, int len)
{
	FIC_DEC_Put((Fic *)data, len);

	return 0;
}

//int msc_callback(HANDLE hDevice, u8 subChId, u8 *data, int len)
int msc_callback(void *hDevice, unsigned char subChId, unsigned char *data, int len)
{
#ifndef USE_FCI_STATIC_MEM
  DMB_INIT_INFO_T *hInit;
  struct list_head *temp;
#endif

  static int First_call_poll_framecnt = 0;

  static int ret_val = 0;

#ifndef USE_FCI_STATIC_MEM
  hInit = (DMB_INIT_INFO_T *)hDevice;
#endif

  ret_val = 0;


  //DEVLOG("msc data[0] = 0x%x, data[1] = 0x%x, data[2] = 0x%x, data[3] = 0x%x \n", data[0],data[1] ,data[2], data[3]);
#if 0
  {    
    //nt i;
    overrun = 0;
    ret_val = BBM_WORD_READ(hDevice, BBM_BUF_OVERRUN, &overrun);

    if( ((ret_val==0) && (overrun&0x01)) || test_overrun>300)
    {
      DMB_OPEN_INFO_T *hOpen;
      hOpen = &m_hOpen;
      hOpen->buf = hopen_buf;

      BBM_WORD_WRITE(hDevice, BBM_BUF_OVERRUN, overrun);
      BBM_WORD_WRITE(hDevice, BBM_BUF_OVERRUN, 0x0000);
      DEVLOG("Overrun occured !!!!!!!!!!  -- by  test_over= %d\n", test_overrun>300 ? 1 : 0);
      test_overrun = 0;
      
      BBM_RESET(hDevice); // 2011.06.28

      fci_ringbuffer_flush(&hOpen->RingBuffer);
      return 0;      
    }
    else if ( ret_val!=0 )
      DEVLOG("Oveerun occured...... return value is not 0 ... val = %d\n", overrun);

    #if 0
    for(i=0;i<len;i+=188)
    {
      if(data[i]!=0x47)
      printk(KERN_ERR "MSC Data 0x%x, 0x%x, 0x%x, 0x%x,    idx : %d\n", data[i], data[i+1], data[i+2], data[i+3], i/188);
    }
    #endif
  }
#endif // del overrun check

  if ( start_service==0 )
  {
    DEVLOG("DMB_ msc_cb::::  start service is NOT ! ignore data_____\n");
    return 0;
  }

  if ( b_First_call_poll )
  {
    First_call_poll_framecnt++;
    //DEVLOG("DMB:: First call poll is 1 , cnt = %d\n", First_call_poll_framecnt);
    if ( First_call_poll_framecnt > 1 )
    {
      DEVLOG("DMB:: First call poll cnt = %d\n", First_call_poll_framecnt);
      b_First_call_poll = 0;
      First_call_poll_framecnt = 0;
    }
    return 0;
  }

#ifndef USE_FCI_STATIC_MEM
  list_for_each(temp, &(hInit->hHead))
  {
    DMB_OPEN_INFO_T *hOpen;

    hOpen = list_entry(temp, DMB_OPEN_INFO_T, hList);

    if(fci_ringbuffer_free(&hOpen->RingBuffer) < len+8 ) 
    {
      return 0;
    }

    FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len >> 8);
    FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len & 0xff);
    fci_ringbuffer_write(&hOpen->RingBuffer, data, len);

    //wake_up_interruptible(&(hOpen->RingBuffer.queue));
  }
#else
{
  DMB_OPEN_INFO_T *hOpen;
  hOpen = &m_hOpen;
  hOpen->buf = hopen_buf;

  //DEVLOG("DMB:: MSC_hOpen addr= %X  , buf= %X  , ring-> = %X\n", hOpen, hOpen->buf, hOpen->RingBuffer);
  
  if(fci_ringbuffer_free(&hOpen->RingBuffer) < len+8 ) 
    {
      DEVLOG("DMB:: MSC_callback()__ ringbuffer_free Error len !!!\n");
      return 0;
    }

    //DEVLOG("DMG:: MSC hOpen.Ringbuffer addr= %X\n", &hOpen->RingBuffer);

    FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len >> 8);
    FCI_RINGBUFFER_WRITE_BYTE(&hOpen->RingBuffer, len & 0xff);
    fci_ringbuffer_write(&hOpen->RingBuffer, data, len);
  }
#endif

  return 0;
}

static int dmbdev_open(struct inode *inode, struct file *file)
{
  DMB_OPEN_INFO_T *hOpen;

  DEVLOG(" dmbdrv OPEN___________________...\n");

#ifndef USE_FCI_STATIC_MEM
  if (!hOpen)
  {
    hOpen = (DMB_OPEN_INFO_T *)kmalloc(sizeof(DMB_OPEN_INFO_T), GFP_KERNEL);
    DEVLOG(" MALLOC()___ hOpen addr= %X  ,size= %d\n", hOpen, sizeof(DMB_OPEN_INFO_T));
    hOpen->buf = NULL;
  }

  if (!hOpen->buf)
  {
    hOpen->buf = (u8 *)kmalloc(RING_BUFFER_SIZE, GFP_KERNEL);
    DEVLOG(" MALLOC()___ hOpen.buf  addr= %X, size= %d\n", hOpen->buf, RING_BUFFER_SIZE);
  }
#else
  hOpen = &m_hOpen;
  hOpen->buf = hopen_buf;
  //DEVLOG(" SALLOC()___ hOpen addr= %X  ,size= %d\n", hOpen, sizeof(DMB_OPEN_INFO_T));
  //DEVLOG(" SALLOC()___ hOpen.buf  addr= %X, size= %d\n", hOpen->buf, RING_BUFFER_SIZE);
#endif

  hOpen->dmbtype = 0;

  list_add(&(hOpen->hList), &(hInit->hHead));

  hOpen->hInit = (HANDLE *)hInit;

  if(hOpen->buf == NULL)
  {
    PRINTF(hInit, "ring buffer malloc error\n");
    return -ENOMEM;
  }

  #if 1
  //if (!dmb_kthread)
  {
    b_exit_dmb_thread = 0;
    dmb_kthread = kthread_run(dmb_thread, (void*)hInit, "dmb_thread");
  }
  #endif

  fci_ringbuffer_init(&hOpen->RingBuffer, hOpen->buf, RING_BUFFER_SIZE);

  //DEVLOG("DMBOPEN: hOpen.RingBuffer addr= %X , hOpen.buf addr= %X  ringbuf-> = %X", &hOpen->RingBuffer, hOpen->buf , hOpen->RingBuffer);

  file->private_data = hOpen;
  kttgenmod_set_device(DEVICE_SPI);
  return 0;
}

static ssize_t dmbdev_read(struct file *file, char __user *buf, size_t count, loff_t *offset)
{
	s32 avail;
	s32 non_blocking = file->f_flags & O_NONBLOCK;
	DMB_OPEN_INFO_T *pDMB = (DMB_OPEN_INFO_T*)file->private_data;
	struct fci_ringbuffer *cibuf = &pDMB->RingBuffer;
	ssize_t len;

	//printk("DMB_READ(): service= %d, count= %d, ring_avail= %d\n", start_service, count, fci_ringbuffer_avail(cibuf) );

  if ( start_service==0 )
  {
    fci_ringbuffer_flush(cibuf);
    return 0;
  }

	if (!cibuf->data || !count)
		return 0;

	if (non_blocking && (fci_ringbuffer_empty(cibuf)))
	{
	  DEVLOG("DMB_READ:: FAIL - block\n");
		return -EWOULDBLOCK;
  }
	
#ifndef DMB_USE_POLL
	if (wait_event_interruptible(cibuf->queue, !fci_ringbuffer_empty(cibuf)))
		return -ERESTARTSYS;
#endif

	avail = fci_ringbuffer_avail(cibuf);

	if (avail < 4)
	{
	  //DEVLOG("DMB_READ:: FAIL ___ avail < 4 \n",avail);
		return 0;
    }

	len = FCI_RINGBUFFER_PEEK(cibuf, 0) << 8;
	len |= FCI_RINGBUFFER_PEEK(cibuf, 1);
//DEVLOG("DMB_READ:: _______ - avail= %d  ,count= %d, len= %d\n",avail, count, len);

	if (avail < len + 2 || count < len)
	//if ( (avail < (len + 2)) || (count < len) )
	{
	  //DEVLOG("DMB_READ:: FAIL - avail= %d  ,count= %d, len= %d\n",avail, count, len);
		return -EINVAL;
  }

	FCI_RINGBUFFER_SKIP(cibuf, 2);

  len = fci_ringbuffer_read_user(cibuf, buf, len);
  //DEVLOG("DMB_READ ___OK:: len= %d\n",len);
  
	return len;
}

static ssize_t dmbdev_write (struct file *file, const char __user *buf, size_t count, loff_t *offset)
{
	DEVLOG("dmbdrv write....\n");
	return 0;
}

#ifdef DMB_USE_POLL
static unsigned int dmbdev_poll( struct file *filp, poll_table *wait )
{
  DMB_OPEN_INFO_T *pDMB = (DMB_OPEN_INFO_T*)filp->private_data;
  struct fci_ringbuffer *cibuf = &pDMB->RingBuffer;

  int mask = 0;
  int buf_len;

  if ( b_First_call_poll )
  {
    ;//DEVLOG("DMB:: First Poll !!! \n");
    //b_First_call_poll = FALSE;
  }

  buf_len= fci_ringbuffer_avail(cibuf);

  if ( buf_len >= (CH0_BUF_LENGTH/2+2) )
  {    
    mask = (POLLIN);
     //DEVLOG("DMB_POLL:: buf is OK...buf= %d  /%d\n", buf_len, CH0_BUF_LENGTH/2+2);
     ReadQ = 0x00;
    return mask;
  }

  

  //	wait_event_interruptible(cibuf->queue, !fci_ringbuffer_empty(cibuf));

  poll_wait( filp, &WaitQueue_Read, wait );

  //	mask = (POLLIN);

  //buf_len= fci_ringbuffer_avail(cibuf);

  if (ReadQ == INT_OCCUR_SIG)
  {
    mask |= (POLLIN);
    //DEVLOG("DMB_POLL:: poll_wait _end -- buf is OK...buf= %d  /%d\n", buf_len, CH0_BUF_LENGTH/2+2);
  }
  else if (ReadQ != 0)
  {
    mask |= POLLERR;
  }
  else
  {
  }

  //DEVLOG("DMB_POLL::  FAIL ...  -- buf_len= %d \n", buf_len);

  ReadQ = 0x00;

  return mask;
}	
#endif

static long dmbdev_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
  s32 ret = BBM_NOK;

  DMB_OPEN_INFO_T *hOpen;
  hOpen = file->private_data;

  switch(cmd)
  {
    case DMB_IOCTL_INIT:
    {
      DEVLOG(" DMB INIT...\n");
      // 2011.06.28
      tdmb_pdata.power(0);
      msleep(5);
      // end__kuzuri
      tdmb_pdata.power(1);
      gInitFlag = 1;
      start_service = 0;
      b_exit_dmb_thread = 0;

      ret = BBM_INIT(hInit);
      ret |= BBM_TUNER_SELECT(hInit, FC8050_TUNER, BAND3_TYPE);

      msleep(10);

      if(ret)
      {
        DEVLOG(" Error: INTERFACE_INIT err..ret=%d\n", ret);
        return -1;
      }

      if(arg==1) // channel search mode
      {
        //??
      }

      #if 0
      if ( tdmb_pdata.irq==NULL )
      {
        DEVLOG("DMB: io_ctrl:: IRQ is NULL, -> request irq...\n");
        tdmb_pdata.irq = MSM_GPIO_TO_INT(DMB_GPIO_INT_N);
        ret = request_irq(tdmb_pdata.irq, dmb_irq, IRQF_TRIGGER_FALLING, TDMB_DEV_NAME, NULL);

        if (ret) 
          DEVLOG("can't get IRQ %d, ret %d\n", tdmb_pdata.irq, ret);

        //enable_irq(tdmb_pdata.irq);
      }
      #endif
    }
    return 0;
    break;

    case DMB_IOCTL_SCAN_ENSEMBLE:
    {
      unsigned int freq;
      ensemble_scan_req_t *pscan=NULL;

      DEVLOG("DMB:: SCAN.  start-----\n");
#ifndef USE_FCI_STATIC_MEM
      if (!pscan)
      pscan = (ensemble_scan_req_t*)kmalloc(sizeof(*pscan), GFP_KERNEL);
      DEVLOG(" MALLOC()___ pScan addr= %X  ,size= %d\n", pscan, sizeof(*pscan));
#else
      pscan = &m_pscan;
      //DEVLOG(" SALLOC()___ pScan addr= %X  ,size= %d\n", pscan, sizeof(*pscan));
#endif      
     

      if(!pscan)
      {
        DEVLOG("Error: memory allocation error.\n");
        ret = -1;
        goto SCAN_ERR;
      }

      ret = copy_from_user(pscan, (void*)arg, sizeof(ensemble_scan_req_t));

      if(ret)
        goto SCAN_ERR;

      freq = TDMB_GET_KOREABAND_FULL_TABLE(pscan->freq);

      if(freq == 0xffff) 
      {
        DEVLOG(" invalid freq index=%d\n", freq);
        ret = -1;
        goto SCAN_ERR;
      }

      hOpen->dmbtype = (u8)FIC_TYPE;

      start_service = 1;
      b_exit_dmb_thread = 0;

        #if 1
        if (!dmb_kthread)
        {
          b_exit_dmb_thread = 0;
          dmb_kthread = kthread_run(dmb_thread, (void*)hInit, "dmb_thread");
        }
        #endif


      if(DMBDrv_ScanCh(freq))
      {
        DEVLOG(" scan fail... freq=%d \n", freq);
        goto SCAN_ERR;
        ret = -1;
      }
      else
      {
        pscan->channel_count = set_ch_data(pscan->channel_data, freq);
        DEVLOG("  scan success freq=%d, found_ch_count=%d \n", freq, pscan->channel_count);
        ret = copy_to_user((void*)arg, pscan, sizeof(*pscan));

        #ifndef KTTECH_FINAL_BUILD
	{
		DMB_CH_DATA_S* ch = pscan->channel_data;
		int i;

		if(pscan->channel_count > 0)
		{
			for(i=0;i<pscan->channel_count;i++, ch++)
			{
				DEVLOG("%d. [%d] %s apptype %d, ch->type  %d, ch->freq  %d, ch->subch_id  %2d, ch->channel_label %s\n",i+1, __LINE__, __func__, ch->tm_id, ch->type, ch->freq, ch->subch_id, ch->channel_label);
			}
		}
	}
        #endif		

        if(ret)
          goto SCAN_ERR;
      }

      SCAN_ERR:				

      #ifndef USE_FCI_STATIC_MEM
      if(pscan)
      {
        DEVLOG(" pscan Error FREE!!!!!\n");
        kfree(pscan);
        pscan = NULL;
      }
      #endif

      hOpen->dmbtype = 0;

      return ret;
    }	

    case DMB_IOCTL_SERVICE_START:
    {
      service_start_req_t serv_req;

      ret = copy_from_user(&serv_req, (void*)arg, sizeof(serv_req));

      DEVLOG("DMB:: Service Start _____________>\n");

      start_service = 0;
      b_exit_dmb_thread = 0;
      #if 1
      if (!dmb_kthread)
      {
        b_exit_dmb_thread = 0;
        dmb_kthread = kthread_run(dmb_thread, (void*)hInit, "dmb_thread");
      }
      #endif
	  //2011.11.28 vincent 삭제 필요
      //msleep(150); // 2011.06.28_temp


      if(ret)
      {
        ret = -1;					
        goto SERVICE_START_ERR;
      }

      fci_ringbuffer_flush(&hOpen->RingBuffer);

      hOpen->dmbtype = MSC_TYPE;
      ret = DMBDrv_SetCh(serv_req.freq, serv_req.subch_id, serv_req.serv_type);

      if(ret != 0)
        hOpen->dmbtype = 0;

      if(ret == -1)
        DEVLOG(" INTERFACE_START Signal Lock Fail \n");
      else if(ret == -2)
        DEVLOG(" INTERFACE_START Sequenc Fail Please DMB Init First!  \n");
      else
      {
        DEVLOG(" INTERFACE_START Success \n");
        start_service = 1;
        b_First_call_poll = 1;
      }

      {
        service_start_req_t *preq;
        preq = (service_start_req_t*)arg;
        DEVLOG("  start result=%d \n", ret);
        ret = copy_to_user(&preq->result, &ret, sizeof(ret));

        if(ret)
          goto SERVICE_START_ERR;

        // // kuzuri.11.25 BBM_WRITE(NULL, BBM_COM_INT_ENABLE, 0x1); // 2011.11.24 kuzuri_test
        
      }
      return ret;

      SERVICE_START_ERR:
      start_service = 0;

      return ret;
    }

    case DMB_IOCTL_SERVICE_END:
      DEVLOG(" DMB: Service END_______________ start service= %d...\n", start_service);
      DMBDrv_SVC_Stop();
      hOpen->dmbtype = 0;

      if ( start_service )
      {
        start_service = 0;
        dmb_isr_sig=1;
        b_exit_dmb_thread = 1;
        wake_up_interruptible(&dmb_isr_wait);
        msleep(30);
      }

      fci_ringbuffer_flush(&hOpen->RingBuffer);

//      BBM_RESET(hInit); // 2011.11.25 - fci
      
      return 0;
      break;

    case DMB_IOCTL_GET_PREBER:
    {
      u32 val;

      if ( b_exit_dmb_thread || (start_service==0))
      {
        DEVLOG(" ioctl get PREBER. But Exit thread is 1 return... thread= %d , start_svc= %d _____\n",  b_exit_dmb_thread, start_service );
        return -1;
      }

      if ( dmb_preber_ready_isr_lock==1 )
      {
        DEVLOG(" ###_____ Ioctl get PREBER. But dmb_ISR_LoCK ==1 . not perform_____\n");
        return -1;
      }

      ret = DMBDrv_FC8050_Get_Viterbi_RT_Ber(&val);

      if(ret)
      {
        DEVLOG(" ioctl get preber ________ FAIL !!! val= %d\n", val);
        return -1;
      }

      if ( 1 ) // val > 500 )
       { DEVLOG(" ioctl get preber=%d\n", val); }
      ret = copy_to_user((unsigned char*)arg, &val, sizeof(val));

      //ret = DMBDrv_FC8050_Get_RSSI(&val);
      //DEVLOG(" ioctl get RSSI __________= %d  -result(%d)\n", val, ret);

      //ret = DMBDrv_FC8050_Get_RS_Ber(&val);

      //DEVLOG(" ioctl get RS_BER __________= %d  -result(%d)\n", val, ret);

      if(ret)
        return -1;
    }
    return 0;

    case DMB_IOCTL_GET_POSTBER:
    {
      u32 val;

      ret = DMBDrv_FC8050_Get_RS_Ber(&val);

      if(ret)
        return -1;

      DEVLOG(" ioctl get postber=%d\n", val);
      ret = copy_to_user((unsigned char*)arg, &val, sizeof(val));

      if(ret)
        return -1;
      }
      return 0;  

    case DMB_IOCTL_GET_CER:
    {
      return -1;
    }
    return 0;

    case DMB_IOCTL_GET_RSSI:
    {
      u32 val;
      u8 ret_val;

      return 0;

      if ( b_exit_dmb_thread || (start_service==0))
      {
        DEVLOG(" ioctl get RSSI. But Exit thread is 1 return... thread= %d , start_svc= %d _____\n",  b_exit_dmb_thread, start_service );
        return -1;
      }

      ret_val = DMBDrv_FC8050_Get_RSSI(&val);      

      DEVLOG(" ioctl get RSSI = %d  -result(%d)\n", val, ret_val);

      val = get_rssi_level(val);

      ret = copy_to_user((unsigned char*)arg, &val, sizeof(val));

      if(ret)
        return -1;
    }
    return 0;

    case DMB_IOCTL_SET_ABORT:
    {
      DEVLOG(" DMB:: ioctl Set Abort....______________...\n");
      b_exit_dmb_thread = 1;
      return 0;
    }

    case DMB_IOCTL_CLOSE:
    {
      DEVLOG("DMB::  io Close ____________ start_flag= %d\n", start_service);
      if ( start_service )
        DMBDrv_SVC_Stop();
      //disable_irq(tdmb_pdata.irq);
      tdmb_pdata.power(0);
      gInitFlag = 0;
      msleep(50);
      return 0;
    }

    case DMB_IOCTL_SET_I2C_RETRY:
    {
      return 0;
    }

    case DMB_IOCTL_SET_I2C_TIMEOUT:
    {
      return 0;
    }

    case DMB_IOCTL_GET_CHIPID:
    {
      u16 nChipID;

      BBM_WORD_READ(hInit, BBM_QDD_CHIP_IDL, &nChipID);

      DEVLOG(" Chip ID =0x%0x\n", nChipID);

      ret = copy_to_user((unsigned char*)arg, &nChipID, sizeof(nChipID));

      if(ret)
        return -1;
    }
    return 0;

#ifdef TDMB_SPI_INTERFACE
		case DMB_IOCTL_STREAM_VALID:
		{
			u32 d_ready;

			if(gpio_get_value(DMB_GPIO_INT_N) == 0)
				d_ready = 1;
			else
				d_ready = 0;

			ret = copy_to_user((unsigned char*)arg, &d_ready, sizeof(d_ready));
			
			if(ret)
				return -1;
		}
		return 0;
#endif

		default:
			DEVLOG(" Error: Unknown command\n");
		break;
	}
	return -1;
}

static int dmbdev_release(struct inode *inode, struct file *file)
{
  DMB_OPEN_INFO_T *hOpen = file->private_data;

  DEVLOG(" DMB:: Release ___________________\n");

  hOpen->dmbtype = 0;

  #if 0
  if ( tdmb_pdata.irq )
  {
    DEVLOG("DMB: Free_IRQ 2-------->>>>\n");
    free_irq(tdmb_pdata.irq, NULL);
    tdmb_pdata.irq = NULL;
  }
  #endif

  if ( start_service )
  {
    start_service = 0;
    dmb_isr_sig=1;
    wake_up_interruptible(&dmb_isr_wait);
    msleep(30);
  }
  #if 0
  if ( dmb_kthread )
  {
    DEVLOG("DMB: release -> Thread kill....\n");
    kthread_stop(dmb_kthread);
    dmb_kthread=NULL;
  }
  #endif

  // 2011.06.28 add __>
  DMBDrv_SVC_Stop();
  //BBM_WRITE(NULL, BBM_COM_STATUS_ENABLE, 0x00);
  msleep(100);
  BBM_DEINIT(NULL);
  // <___

  list_del(&(hOpen->hList));

  #ifndef USE_FCI_STATIC_MEM
  DEVLOG(" hOpen FREE+++\n");
  DEVLOG(" hOpen.buf  FREE+++++\n");

  kfree(hOpen->buf);
  hOpen->buf = NULL;
  kfree(hOpen);
  hOpen = NULL;
  #endif

  kttgenmod_clear_device(DEVICE_SPI);
  DEVLOG(" dmbdrv release...\n");

  return 0;
}

static int set_ch_data(DMB_CH_DATA_S* pchbuf, unsigned int freq)
{
	int i;
	int dmbchcnt, dabchcnt, chcnt;
	DMB_CH_DATA_S* pch;
	char*  ensemble_label;
	char* service_label;
	SubChInfoTypeDB* DMBSubChInfo;
	SubChInfoTypeDB* DABSubChInfo;
	pch = pchbuf;

	dmbchcnt = DMBDrv_GetDMBSubChCnt();
	dabchcnt = DMBDrv_GetDABSubChCnt();
	
	for(i=0;i<dmbchcnt;i++)
	{
		DMBSubChInfo = DMBDrv_GetFICDMB(i);
		pch->freq = freq;
		ensemble_label = DMBDrv_GetEnsembleLabel(&pch->ensemble_id);
		pch->subch_id = DMBSubChInfo->ucSubchID;
		pch->type = DMBSubChInfo->ucServiceType;
		pch->tm_id = DMBSubChInfo->ucTMId;
		pch->bit_rate = DMBDrv_GetSubchBitRate(DMBSubChInfo->ucSubchID);
		service_label = DMBDrv_GetSubChDMBLabel(i);
		strncpy(&pch->ensemble_label[0], ensemble_label, sizeof(pch->ensemble_label));
		strncpy(&pch->channel_label[0], service_label, sizeof(pch->channel_label));
		pch++;
	}
	
	for(i=0;i<dabchcnt;i++)
	{
		DABSubChInfo = DMBDrv_GetFICDAB(i);
		pch->freq = freq;
		ensemble_label = DMBDrv_GetEnsembleLabel(&pch->ensemble_id);
		pch->subch_id = DABSubChInfo->ucSubchID;
		pch->type = DABSubChInfo->ucServiceType;
		pch->tm_id = DABSubChInfo->ucTMId;
		pch->bit_rate = DMBDrv_GetSubchBitRate(DABSubChInfo->ucSubchID);
		service_label = DMBDrv_GetSubChDABLabel(i);
		strncpy(&pch->ensemble_label[0], ensemble_label, sizeof(pch->ensemble_label));
		strncpy(&pch->channel_label[0], service_label, sizeof(pch->channel_label));
		pch++;
	}
	
	chcnt = dmbchcnt + dabchcnt;
	return chcnt;
}

static int get_rssi_level(unsigned int rssi)
{
  if(rssi > KXDMB_RSSI_LEVEL_0_MAX-KX_DMB_LNA_GAIN)
  {
    return KXDMB_RSSI_LEVEL_0;
  } 
  else if(rssi > KXDMB_RSSI_LEVEL_1_MAX-KX_DMB_LNA_GAIN)
  {
    return KXDMB_RSSI_LEVEL_1;
  }
  else if(rssi > KXDMB_RSSI_LEVEL_2_MAX-KX_DMB_LNA_GAIN)
  {
    return KXDMB_RSSI_LEVEL_2;
  }
  else if(rssi > KXDMB_RSSI_LEVEL_3_MAX-KX_DMB_LNA_GAIN)
  {
    return KXDMB_RSSI_LEVEL_3;
  }
  else if(rssi > KXDMB_RSSI_LEVEL_4_MAX-KX_DMB_LNA_GAIN)
  {
    return KXDMB_RSSI_LEVEL_4;
  }
  else if(rssi > KXDMB_RSSI_LEVEL_5_MAX-KX_DMB_LNA_GAIN)
  {
    return KXDMB_RSSI_LEVEL_5;
  }
  else
  {
    return KXDMB_RSSI_LEVEL_6;
  }
}

#ifdef TDMB_SPI_INTERFACE_TEST
int spi_interface_test(void)
{
	u16 i; 
	u16 wdata = 0; 
	u32 ldata = 0; 
	u8 data = 0;
	u8 temp = 0;
	s32 ret = BBM_NOK;
	u16 nChipID;

	ret = BBM_INIT(hInit);
	ret |= BBM_TUNER_SELECT(hInit, FC8050_TUNER, BAND3_TYPE);

	if(ret)
	{
		DEVLOG(" Error: INTERFACE_INIT err..ret=%d\n", ret);
	}
	
	if(BBM_PROBE(NULL))
	{
		DEVLOG("FC8050 Probe Fail !!!\n");
	}

	for(i=0;i<20;i++)
	{
		BBM_WRITE(NULL, 0x05, i & 0xff);
		BBM_READ(NULL, 0x05, &data);
		if((i & 0xff) != data)
			DEVLOG("FC8050 byte test (0x%x,0x%x)\n", i & 0xff, data);
	}
	
	for(i=0;i<20;i++)
	{
		BBM_WORD_WRITE(NULL, 0x0210, i & 0xffff);
		BBM_WORD_READ(NULL, 0x0210, &wdata);
		if((i & 0xffff) != wdata)
			DEVLOG("FC8050 word test (0x%x,0x%x)\n", i & 0xffff, wdata);
	}
	
	for(i=0;i<20;i++)
	{
		BBM_LONG_WRITE(NULL, 0x0210, i & 0xffffffff);
		BBM_LONG_READ(NULL, 0x0210, &ldata);
		if((i & 0xffffffff) != ldata)
			DEVLOG("FC8050 long test (0x%x,0x%x)\n", i & 0xffffffff, ldata);
	}

	for(i=0;i<100;i++)
	{
		temp = i&0xff;
		BBM_TUNER_WRITE(NULL, 0x12, 0x01, &temp, 0x01);
		BBM_TUNER_READ(NULL, 0x12, 0x01, &data, 0x01);
		if((i & 0xff) != data)
			DEVLOG("______________________ FC8050 tuner test (0x%x,0x%x)\n", i & 0xff, data);
	}
	temp = 0x51;
	BBM_TUNER_WRITE(NULL, 0x12, 0x01, &temp, 0x01 );	

    BBM_WRITE(NULL, BBM_BUF_MISC_CTRL, 0x19);
	BBM_WORD_READ(NULL, BBM_QDD_CHIP_IDL, &nChipID);
	DEVLOG(" Chip ID =0x%0x\n", nChipID);
	DEVLOG(" [Interface Test][SUCCESS]: OK \r\n");
	return 1;
}

static void dmb_test(void)
{
  #ifdef TDMB_SPI_INTERFACE_TEST
  tdmb_pdata.power(0);
  msleep(5);
  tdmb_pdata.power(1);
	//msleep(5);
	//tdmb_pdata.power(0);
	//tdmb_reset(0);
    	//tdmb_demod_enable(0);
    	//msleep(5);
    	//tdmb_reset(1);
      //tdmb_demod_enable(1);
	#endif

  spi_interface_test();    
}
#endif

#ifdef TDMB_SPI_INTERFACE
static int tdmb_spi_probe(struct spi_device *spi)
{
  s32 ret;

  //spi->max_speed_hz =  10800000; //8000000;//4000000;
  spi->max_speed_hz =  10800000; //5400000;//8000000;//10800000; //8000000;//4000000;
  spi->mode = 0;
  spi->bits_per_word = 8;

  fc8050_spi = spi;

  ret = spi_setup(spi);
  DEVLOG("[%s] : spi_setup ret= %d\n", __func__, (int) ret );

  DEVLOG(" [%s] : spi.cs[%d], bit_per_word[%d]  mod[%d], hz[%d] \n", 
  spi->modalias, spi->chip_select, spi->bits_per_word, spi->mode, spi->max_speed_hz);

  #ifdef TDMB_SPI_INTERFACE_TEST
  dmb_test();
  #endif

  #if 1
  tdmb_pdata.irq = MSM_GPIO_TO_INT(DMB_GPIO_INT_N);
  ret = request_irq(tdmb_pdata.irq, dmb_irq, IRQF_TRIGGER_FALLING, TDMB_DEV_NAME, NULL);

  if (ret)
  {
    free_irq(tdmb_pdata.irq, 0);
    DEVLOG("can't get IRQ %d, ret %d\n", tdmb_pdata.irq, ret);
    return -EINVAL;
  }
  #endif

  return 0;
}

static struct spi_driver tdmb_spi = {
	.driver = {
		.name = 	TDMB_DEV_NAME,
		.owner =	THIS_MODULE,
	},
	.probe =	tdmb_spi_probe,
};
#endif

static const struct file_operations dmbdev_fops = {
  .owner		= THIS_MODULE,
  .llseek		= no_llseek,
  .read		= dmbdev_read,
  .write		= dmbdev_write,
  .poll		= dmbdev_poll,
  .unlocked_ioctl	= dmbdev_ioctl,
  .open		= dmbdev_open,
  .release	= dmbdev_release,
};

static int __init dmb_init_module(void)
{
	int ret;

	DEVLOG(" dmbdrv module start....\n");

	tdmb_pdata.setup();
#if 0 //def TDMB_SPI_INTERFACE_TEST
	tdmb_pdata.power(1);
	//msleep(5);
	//tdmb_pdata.power(0);
	tdmb_reset(0);
    	tdmb_demod_enable(0);
    	msleep(5);
    	tdmb_reset(1);
      tdmb_demod_enable(1);
#endif

	ret = register_chrdev(DMB_MAJOR, TDMB_DRV_NAME, &dmbdev_fops); // if success, return 0
	if (ret < 0) {
		DEVLOG(" [%s] unable to get major %d for fb devs\n", __func__, DMB_MAJOR);
		return ret;
	}

	dmbclass = class_create(THIS_MODULE, TDMB_CLASS_NAME);
	if (IS_ERR(dmbclass)) {
		DEVLOG(" [%s] Unable to create dmbclass; errno = %ld\n", __func__, PTR_ERR(dmbclass));
		unregister_chrdev(DMB_MAJOR, TDMB_DRV_NAME);
		return PTR_ERR(dmbclass);
	}

	dmbdev = device_create(dmbclass, NULL, MKDEV(DMB_MAJOR, 26), NULL, TDMB_DEV_NAME);
	
	if (IS_ERR(dmbdev)) {
		DEVLOG(" [%s] Unable to create device for framebuffer ; errno = %ld\n",	__func__, PTR_ERR(dmbdev));
		unregister_chrdev(DMB_MAJOR, TDMB_DRV_NAME);
		return PTR_ERR(dmbdev);
	}

	DMBDrv_init();

	#ifdef DMB_USE_POLL
  init_waitqueue_head(&WaitQueue_Read);
  #endif
	
#ifdef TDMB_SPI_INTERFACE
	ret = spi_register_driver(&tdmb_spi);

	if (ret < 0) {
		DEVLOG( " [%s] Unable spi_register_driver; result = %d\n", __func__, ret);
		class_destroy(dmbclass);
		unregister_chrdev(DMB_MAJOR,TDMB_DRV_NAME);
	}
#endif


	DEVLOG(" %s Success!\n", __func__);
	return 0;
}

static void __exit dmb_cleanup_module(void)
{
  DEVLOG(" dmbdrv module exit.....\n");

  DMBDrv_exit();  
  device_destroy(dmbclass, MKDEV(DMB_MAJOR,0));
  class_destroy(dmbclass);
  unregister_chrdev(DMB_MAJOR, TDMB_DRV_NAME);
}	

module_init(dmb_init_module);
module_exit(dmb_cleanup_module);
MODULE_AUTHOR("KTTech. <xxx@kttech.co.kr>");
MODULE_DESCRIPTION("dmb /dev entries driver");
MODULE_LICENSE("GPL");
