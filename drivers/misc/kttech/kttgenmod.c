// KTTech module for general purpose

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/sched.h>
#include <asm/io.h>
#include <linux/mfd/pm8xxx/misc.h>
#include <linux/init.h>
#include <linux/rtc.h>

#include <linux/kttgenmod.h>

#ifndef KTTECH_FINAL_BUILD
//#define WATCHDOG_TEST
#endif

#ifdef WATCHDOG_TEST
#include <linux/workqueue.h>
#endif

#define BITS_PART16(x, high, low) (( x >> low )  & ( 0xffff >> (32-high-1+low) ))
#define BITS_PART32(x, high, low) (( x >> low )  & ( 0xffffffff >> (32-high-1+low) ))


static u32 para_chiprev;
static uint msm_version;
static long rooting_sec;

static int __init rooting_setup(char *str)
{
	unsigned long sec;

	if(!strict_strtoul(str, 0, &sec))
	{
		rooting_sec = sec;
	}
	return 1;
}

__setup("androidboot.rooting=", rooting_setup);

static ssize_t rooting_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
	int count = 0;

	if(rooting_sec){
		struct rtc_time tm;

		rtc_time_to_tm(rooting_sec, &tm);
		count += sprintf(buf, "%d-%02d-%02d %02d:%02d:%02d\n",
			tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
			tm.tm_hour+9, tm.tm_min, tm.tm_sec);
	} else {
		count += sprintf(buf, "No Rooting\n");
	}

	return count;
}
static ssize_t stack_debug_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
	int count = 0;

	count += sprintf(buf, "stack dump\n");
	return count;
}

static ssize_t stack_debug_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int level;
	struct task_struct *tsk;

	if (sscanf(buf, "%u", &level) != 1) {
		printk("Invalid values\n");
		return -EINVAL;
	}

	if(level == 1) {
		printk(" ========== Stack DUMP =============\n");

		for_each_process(tsk) {
			printk(KERN_INFO "\nPID: %d, Name: %s\n",
					tsk->pid, tsk->comm);
			show_stack(tsk, NULL);
		}
	}

	return count;
}

struct kobject *kttech_kobj;

static DEVICE_ATTR(tsk_stack_dump, 0600, stack_debug_show, stack_debug_store);
static DEVICE_ATTR(rooting, 0444, rooting_show, NULL);

static kttgenmod_device_t busy_device = 0;
static ssize_t busy_device_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
	int len = 0;

	if(!buf)
	{
		printk(KERN_INFO "%s: Invalid buf !!! \n", __func__);
		return 0;
	}
	
	len = sprintf(buf, "%d\n", busy_device);
	if (len < 0)
	{
		printk(KERN_INFO "%s: Invalid len !!! \n", __func__);
		return 0;
	}
	
	return len;
}

static ssize_t busy_device_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t count)
{
	kttgenmod_device_t  device;

	if (sscanf(buf, "%u", &device) != 1) {
		printk("Invalid values\n");
		return -EINVAL;
	}

	busy_device = device;

	return count;
}

struct kobject *kttech_kobj;

static DEVICE_ATTR(busy_device, 0600, busy_device_show, busy_device_store);


#ifdef WATCHDOG_TEST
static void watchdog_test_work(struct work_struct *work);
static DECLARE_DELAYED_WORK(watchdog_test, watchdog_test_work);
static void watchdog_test_work(struct work_struct *work)
{
	printk("%s watchdog test enter while  \n",__func__);
	while(1) {}
}


static int  enable_watchdog_test= 0;
static ssize_t watchdog_test_show(struct device *dev,
            struct device_attribute *attr, char *buf)
{
	int len = 0;

	if(!buf)
	{
		printk(KERN_INFO "%s: Invalid buf !!! \n", __func__);
		return 0;
	}
	
	len = sprintf(buf, "%d\n", enable_watchdog_test);
	if (len < 0)
	{
		printk(KERN_INFO "%s: Invalid len !!! \n", __func__);
		return 0;
	}
	
	return len;
}

static ssize_t watchdog_test_store(struct device *dev,
            struct device_attribute *attr, const char *buf, size_t count)
{
	int  enable;

	if (sscanf(buf, "%d", &enable) != 1) {
		printk("Invalid values\n");
		return -EINVAL;
	}

	enable_watchdog_test = enable;

	schedule_delayed_work_on(0, &watchdog_test, msecs_to_jiffies(1000));
	
	return count;
}

struct kobject *kttech_kobj;

static DEVICE_ATTR(watchdog_test, 0666, watchdog_test_show, watchdog_test_store);
#endif

static struct attribute *stack_attrs[] = {
    &dev_attr_tsk_stack_dump.attr,
    &dev_attr_busy_device.attr,
    &dev_attr_rooting.attr,
#ifdef WATCHDOG_TEST    
    &dev_attr_watchdog_test.attr, 
#endif    
    NULL
};

static const struct attribute_group stack_attr_group = {
    .attrs = stack_attrs,
};

static int __init kttpre_init(void)
{
	u32* up;
	int ret = 0;

	printk("### %s, \n", __func__);
	up = (u32*)ioremap(0x00802054, 0x16);
	para_chiprev = *up;
	printk("	chip rev=%08x", para_chiprev);	

	msm_version = BITS_PART32(para_chiprev, 31, 28);

	// refer : DEVICE REVISION GUIDLE 
	printk("    Manufacture ID code = %0x \n", BITS_PART32(para_chiprev, 11,  1));
	printk("    Part Number         = %0x \n", BITS_PART32(para_chiprev, 27, 12));
	printk("    Version Data        = %0x \n", msm_version);

	iounmap(up);

	kttech_kobj = kobject_create_and_add("kttech_misc", NULL);
    if(!kttech_kobj) {
        printk(KERN_ERR "pwk: can not create stack_dump sys file\n");
    } else {
        ret = sysfs_create_group(kttech_kobj, &stack_attr_group);
    }
	
	return ret;
}

static int __init kttlate_init(void)
{	
	//int ret;

	struct pm8xxx_coincell_chg coincell_config;

	printk("### %s, \n", __func__);

          // gun@kttech.co.kr  - moved SMPL configuration into SBL3
	// SMPL configuration
	//ret = pm8xxx_smpl_set_delay(PM8XXX_SMPL_DELAY_1p5);
	//printk("***    smpl set delay ret = %d \n", ret);
	//ret = pm8xxx_smpl_control(0); // disable SMPL
	//printk("***    smpl control ret = %d \n", ret);
	// gun@kttech.co.kr  - moved SMPL configuration into SBL3

	pm8xxx_coincell_chg_config(&coincell_config);

	printk("coincell charger configuration = state: %d, voltage: 0x%x, resister: 0x%x\n",
	  coincell_config.state, coincell_config.voltage, coincell_config.resistor);

	return 0;

}

uint get_msmchip_id(void)
{
	return para_chiprev;
}

uint get_msmchip_ver(void)
{
	return msm_version;
}

void kttgenmod_set_device(kttgenmod_device_t device)
{
	busy_device |= device;
}
EXPORT_SYMBOL(kttgenmod_set_device);

void kttgenmod_clear_device(kttgenmod_device_t device)
{
	busy_device &= ~device;
}
EXPORT_SYMBOL(kttgenmod_clear_device);

module_param(msm_version, uint, 0644);
MODULE_LICENSE("GPL");
arch_initcall(kttpre_init);
late_initcall(kttlate_init);
