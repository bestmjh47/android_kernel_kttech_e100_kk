/*
 * Copyright (C) 2010 NXP Semiconductors
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/jiffies.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/spinlock.h>
#include <linux/nfc/pn544-kttech.h>

#ifdef NFC_USE_MSM_XO
#include <mach/msm_xo.h>
#endif

// KTT_UPDATE : FEATURE_KTTECH_NFC
// modify : kkong120322
// description :
// S=====================================================================
typedef enum{
    KTTECH_CMD_SET_LOG_ENABLE,
    KTTECH_CMD_SET_LOG_DISABLE,
    KTTECH_CMD_SET_CLK_REQ_PIN_MODE,
    KTTECH_CMD_END
}KTTECH_CMD_TYPE;
// E=====================================================================

#ifdef NFC_USE_MSM_XO
static struct msm_xo_voter *nfc_clock;
#endif

#define _PN544_DEV_ERROR_FIX
#define _PN544_I2C_LOG_VIEW
//#define _PN544_I2C_TEST

#define MAX_BUFFER_SIZE	512
#ifdef _PN544_I2C_LOG_VIEW
#define MAX_PRINTABLE_BUFFER_SIZE	64
static bool log_enable = false;
#endif

#define READ_IRQ_MODIFY

#ifdef READ_IRQ_MODIFY
bool do_reading = false;
static bool cancle_read = false;
#endif

struct pn544_dev	{
	wait_queue_head_t	read_wq;
	struct mutex		read_mutex;
	struct i2c_client	*client;
	struct miscdevice	pn544_device;
	unsigned int 		ven_gpio;
	unsigned int 		firm_gpio;
	unsigned int		irq_gpio;
	bool			irq_enabled;
	spinlock_t		irq_enabled_lock;
};

static struct pn544_i2c_platform_data *platform_data;
#ifdef _PN544_DEV_ERROR_FIX
static struct pn544_dev *g_pn544_dev;
#endif
  
static void pn544_disable_irq(struct pn544_dev *pn544_dev)
{
    unsigned long flags;

    spin_lock_irqsave(&pn544_dev->irq_enabled_lock, flags);
    if (pn544_dev->irq_enabled) {
        disable_irq_nosync(pn544_dev->client->irq);
        pn544_dev->irq_enabled = false;
    }
    spin_unlock_irqrestore(&pn544_dev->irq_enabled_lock, flags);
}

static irqreturn_t pn544_dev_irq_handler(int irq, void *dev_id)
{
    struct pn544_dev *pn544_dev = dev_id;

    pn544_disable_irq(pn544_dev);
#ifdef READ_IRQ_MODIFY
    do_reading=1;
#endif

#ifdef _PN544_I2C_LOG_VIEW
    if(log_enable)
        printk(KERN_INFO "%s :\n", __func__);
#endif

    /* Wake up waiting readers */
    wake_up(&pn544_dev->read_wq);

    return IRQ_HANDLED;
}

static ssize_t pn544_dev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *offset)
{
    struct pn544_dev *pn544_dev = filp->private_data;
    char tmp[MAX_BUFFER_SIZE];
    int ret;

    if (count > MAX_BUFFER_SIZE)
    count = MAX_BUFFER_SIZE;

    pr_debug("%s : reading %zu bytes.\n", __func__, count);

    mutex_lock(&pn544_dev->read_mutex);

    if (!gpio_get_value(pn544_dev->irq_gpio)) {
        if (filp->f_flags & O_NONBLOCK) {
            ret = -EAGAIN;
            goto fail;
        }

        pn544_dev->irq_enabled = true;

#ifdef READ_IRQ_MODIFY
        do_reading=0;
#endif

        enable_irq(pn544_dev->client->irq);
#ifdef READ_IRQ_MODIFY
        ret = wait_event_interruptible(pn544_dev->read_wq, do_reading);
#else
        ret = wait_event_interruptible(pn544_dev->read_wq,
        gpio_get_value(pn544_dev->irq_gpio));
#endif
        pn544_disable_irq(pn544_dev);

#ifdef READ_IRQ_MODIFY
        if(cancle_read == true)
        {
            cancle_read = false;
            ret = -1;
            goto fail;
        }
#endif
        if (ret)
        goto fail;

    }

    /* Read data */
    ret = i2c_master_recv(pn544_dev->client, tmp, count);
    mutex_unlock(&pn544_dev->read_mutex);

#ifdef _PN544_I2C_LOG_VIEW
    if(log_enable)
    {
        int i;
        char print_tmp[MAX_PRINTABLE_BUFFER_SIZE];
        char *p_print_tmp = print_tmp;

        printk(KERN_INFO "%s Data: (%zu bytes)\n", __func__, count);
        memset((char*)print_tmp, 0, sizeof(print_tmp));
        for(i=0; i<count; i++)
        {
            sprintf(p_print_tmp, " %02x", tmp[i]);
            p_print_tmp += (sizeof(char)*3);
            if(i%20 == 0 && i !=0)
            {
                printk(KERN_INFO "%s", print_tmp);
                memset((char*)print_tmp, 0, sizeof(print_tmp));
                p_print_tmp = print_tmp;
            }
        }
        if(0 != print_tmp[0])
            printk(KERN_INFO "%s", print_tmp);

        printk(KERN_INFO "ven:%d(%x), irq:%d(%x), firm:%d(%x)", pn544_dev->ven_gpio, gpio_get_value(pn544_dev->ven_gpio), 
                                                                                                    pn544_dev->irq_gpio, gpio_get_value(pn544_dev->irq_gpio), 
                                                                                                    pn544_dev->firm_gpio, gpio_get_value(pn544_dev->firm_gpio));
    }
#endif

    if (ret < 0) {
        printk(KERN_ERR "%s: fail!!!!!!i2c_master_recv returned %d\n", __func__, ret);
        return ret;
    }
    if (ret > count) {
        printk(KERN_ERR "%s: fail!!!!!!received too many bytes from i2c (%d)\n", __func__, ret);
        return -EIO;
    }
    if (copy_to_user(buf, tmp, ret)) {
        printk(KERN_ERR "%s : failed to copy to user space\n", __func__);
        return -EFAULT;
    }
    return ret;

fail:
    mutex_unlock(&pn544_dev->read_mutex);
    return ret;
}

static ssize_t pn544_dev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *offset)
{
    struct pn544_dev  *pn544_dev;
    char tmp[MAX_BUFFER_SIZE];
    int ret;

    pn544_dev = filp->private_data;

    if (count > MAX_BUFFER_SIZE)
        count = MAX_BUFFER_SIZE;

    if (copy_from_user(tmp, buf, count)) {
        printk(KERN_INFO "%s : failed to copy from user space\n", __func__);
        return -EFAULT;
    }

#ifdef _PN544_I2C_LOG_VIEW
    if(log_enable)
    {
        int i;
        char print_tmp[MAX_PRINTABLE_BUFFER_SIZE];
        char *p_print_tmp = print_tmp;

        printk(KERN_INFO "%s Data: (%zu bytes)\n", __func__, count);
        memset((char*)print_tmp, 0, sizeof(print_tmp));
        for(i=0; i<count; i++)
        {
            sprintf(p_print_tmp, " %02x", tmp[i]);
            p_print_tmp += (sizeof(char)*3);
            if(i%20 == 0 && i !=0)
            {
                printk(KERN_INFO "%s", print_tmp);
                memset((char*)print_tmp, 0, sizeof(print_tmp));
                p_print_tmp = print_tmp;
            }
        }
        if(0 != print_tmp[0])
            printk(KERN_INFO "%s", print_tmp);

        printk(KERN_INFO "ven:%d(%x), irq:%d(%x), firm:%d(%x)", pn544_dev->ven_gpio, gpio_get_value(pn544_dev->ven_gpio), 
                                                                                                  pn544_dev->irq_gpio, gpio_get_value(pn544_dev->irq_gpio), 
                                                                                                  pn544_dev->firm_gpio, gpio_get_value(pn544_dev->firm_gpio));
    }
#endif

    /* Set power : (12.02.06) debugged for i2c failed message at suspend/resume. */
    //gpio_set_value(pn544_dev->firm_gpio, 0);
    //gpio_set_value(pn544_dev->ven_gpio, 1);
    //msleep(10);

    /* Write data */
    ret = i2c_master_send(pn544_dev->client, tmp, count);
    if (ret != count) {
#ifdef _PN544_I2C_LOG_VIEW		
    if(log_enable)
        printk(KERN_ERR "%s error!!!!!!!!(%d)\n", __func__, ret);
#endif		
        ret = -EIO;
    }
    return ret;
}

#ifdef _PN544_I2C_TEST
static int pn544_dev_test(struct i2c_client *client)
{
    int ret;
    char tmpTx[6];
    char tmpRx[4];

    printk(KERN_ERR "%s :\n", __func__);

    tmpTx[0]=0x05;
    tmpTx[1]=0xF9;
    tmpTx[2]=0x04;
    tmpTx[3]=0x00;
    tmpTx[4]=0xC3;
    tmpTx[5]=0xE5;

    ret = i2c_master_send(client, tmpTx, 6);
    if (ret < 0) {
        printk(KERN_ERR "%s : i2c_master_send returned %d\n", __func__, ret);
        ret = -EIO;
    }

    /* Read data */
    ret = i2c_master_recv(client, tmpRx, 4);
    if (ret < 0) {
        printk(KERN_ERR "%s : i2c_master_recv returned %d\n", __func__, ret);
        ret = -EIO;
    }

    printk(KERN_ERR "%s : get data %d %d %d %d\n", __func__, tmpRx[0], tmpRx[1], tmpRx[2], tmpRx[3]);

    return ret;
}
#endif

static int pn544_dev_open(struct inode *inode, struct file *filp)
{
#ifdef _PN544_DEV_ERROR_FIX
    filp->private_data = g_pn544_dev;

#ifdef _PN544_I2C_LOG_VIEW
    if(log_enable)
    {
        printk(KERN_INFO "ven:%d(%x), irq:%d(%x), firm:%d(%x)", g_pn544_dev->ven_gpio, gpio_get_value(g_pn544_dev->ven_gpio), 
                                                                                                      g_pn544_dev->irq_gpio, gpio_get_value(g_pn544_dev->irq_gpio), 
                                                                                                      g_pn544_dev->firm_gpio, gpio_get_value(g_pn544_dev->firm_gpio));
    }
#endif
#else
    struct pn544_dev *pn544_dev = container_of(filp->private_data,
                                                        			struct pn544_dev,
                                                        			pn544_device);

    filp->private_data = pn544_dev;
#endif

    printk(KERN_INFO "%s : %d,%d\n", __func__, imajor(inode), iminor(inode));

    return 0;
}

// KTT_UPDATE : FEATURE_KTTECH_NFC
// modify : kkong120322
// description : 
// S=====================================================================
static int pn544_kttech_cmd_process(unsigned long command)
{
    int ret = 0;

    if(log_enable) {
        printk(KERN_INFO "%s:  command(%ld)\n", __func__, command);
    }

    switch(command)
    {
        case KTTECH_CMD_SET_LOG_ENABLE:
            log_enable = true;
            break;

        case KTTECH_CMD_SET_LOG_DISABLE:
            log_enable = false;
            break;

#ifdef NFC_USE_MSM_XO
        case KTTECH_CMD_SET_CLK_REQ_PIN_MODE:
            ret = msm_xo_mode_vote(nfc_clock, MSM_XO_MODE_PIN_CTRL);
            if (ret < 0) {
                printk(KERN_ERR "%s:  Failed to vote for TCX0_D1 ON (%d)\n", __func__, ret);
            }
            break;
#endif

        default:
            printk(KERN_ERR "%s:  Invalid Parameter (%ld)\n", __func__, command);
            break;
    }

    return ret;
}
// E=====================================================================

static long pn544_dev_ioctl(struct file *filp,
		unsigned int cmd, unsigned long arg)
{
    struct pn544_dev *pn544_dev = filp->private_data;

    switch (cmd) {
        case PN544_SET_PWR:
            if (arg == 2) {
                /* power on with firmware download (requires hw reset)
                */
                printk(KERN_INFO "%s power on with firmware\n", __func__);
                gpio_set_value(pn544_dev->ven_gpio, 1);
                gpio_set_value(pn544_dev->firm_gpio, 1);
                msleep(20);
                gpio_set_value(pn544_dev->ven_gpio, 0);
                msleep(60);
                gpio_set_value(pn544_dev->ven_gpio, 1);
                msleep(20);
            } else if (arg == 1) {
                /* power on */
                printk(KERN_INFO "%s power on, gpio:[%d] [%d]\n", __func__, pn544_dev->firm_gpio, pn544_dev->ven_gpio);
                gpio_set_value(pn544_dev->firm_gpio, 0);
                gpio_set_value(pn544_dev->ven_gpio, 1);
                msleep(20);
            } else  if (arg == 0) {
                /* power off */
                printk(KERN_INFO "%s power off\n", __func__);
                gpio_set_value(pn544_dev->firm_gpio, 0);
                gpio_set_value(pn544_dev->ven_gpio, 0);
                msleep(60);
#ifdef READ_IRQ_MODIFY
            } else if (arg == 3) {
                pr_info("%s Read Cancle\n", __func__);
                cancle_read = true;
                do_reading = 1;
                wake_up(&pn544_dev->read_wq);
#endif
            } else {
                printk(KERN_INFO "%s bad arg\n", __func__);
                return -EINVAL;
            }
            break;

        // KTT_UPDATE : FEATURE_KTTECH_NFC
        // modify : kkong120322
        // description : 
        // S=====================================================================
#ifdef NFC_USE_MSM_XO
        case KTTECH_PN544_COMMAND:
            pn544_kttech_cmd_process(arg);
            break;
#endif
        // E=====================================================================

        default:
            printk(KERN_INFO "%s bad ioctl %d\n", __func__, cmd);
            return -EINVAL;
    }

    return 0;
}

static const struct file_operations pn544_dev_fops = {
	.owner	= THIS_MODULE,
	.llseek	= no_llseek,
	.read	= pn544_dev_read,
	.write	= pn544_dev_write,
	.open	= pn544_dev_open,
	.unlocked_ioctl  = pn544_dev_ioctl,
};

static int pn544_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
    int ret;
    //struct pn544_i2c_platform_data *platform_data;
    struct pn544_dev *pn544_dev;

    printk("***************** pn544_probe *****************\n");
    
    platform_data = client->dev.platform_data;
    
    if (platform_data == NULL) {
        printk(KERN_INFO "%s : nfc probe fail\n", __func__);
        return  -ENODEV;
    }

#ifdef _PN544_I2C_LOG_VIEW
    if(log_enable)
        printk(KERN_INFO "%s : nfc probe start, firm_gpio:%d, ven_gpio:%d\n", __func__, gpio_get_value(platform_data->firm_gpio), gpio_get_value(platform_data->ven_gpio));
#endif

#ifdef NFC_USE_MSM_XO
    nfc_clock = msm_xo_get(MSM_XO_TCXO_D1, "nfc_clock");
    if (IS_ERR(nfc_clock)) {
        ret = PTR_ERR(nfc_clock);
        printk(KERN_ERR "%s: Couldn't get TCXO_D1 vote for NFC (%d)\n", __func__, ret);
    }
#endif

    platform_data->setup_power(&client->dev);
    platform_data->setup_gpio(1);

    printk(KERN_INFO "nfc probe platform data (irq%d:%d) (ven%d:%d) (firm%d:%d)\n",
                                    platform_data->irq_gpio, gpio_get_value(platform_data->irq_gpio), 
                                    platform_data->ven_gpio, gpio_get_value(platform_data->ven_gpio),
                                    platform_data->firm_gpio, gpio_get_value(platform_data->firm_gpio));

    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        printk(KERN_INFO "%s : need I2C_FUNC_I2C\n", __func__);
        return  -ENODEV;
    }

#if 0    //kkong110316
    ret = gpio_request(platform_data->irq_gpio, "nfc_int");
    if (ret)
        return  -ENODEV;
    ret = gpio_request(platform_data->ven_gpio, "nfc_ven");
    if (ret)
        goto err_ven;
    ret = gpio_request(platform_data->firm_gpio, "nfc_firm");
    if (ret)
        goto err_firm;
#endif

    pn544_dev = kzalloc(sizeof(*pn544_dev), GFP_KERNEL);
    if (pn544_dev == NULL) {
        dev_err(&client->dev,
                        "failed to allocate memory for module data\n");
                        ret = -ENOMEM;
        goto err_exit;
    }

    dev_info(&client->adapter->dev, "detected pn544\n");

    pn544_dev->irq_gpio = platform_data->irq_gpio;
    pn544_dev->ven_gpio  = platform_data->ven_gpio;
    pn544_dev->firm_gpio  = platform_data->firm_gpio;
    pn544_dev->client   = client;

    /* init mutex and queues */
    init_waitqueue_head(&pn544_dev->read_wq);
    mutex_init(&pn544_dev->read_mutex);
    spin_lock_init(&pn544_dev->irq_enabled_lock);

    pn544_dev->pn544_device.minor = MISC_DYNAMIC_MINOR;
    pn544_dev->pn544_device.name = "pn544";
    pn544_dev->pn544_device.fops = &pn544_dev_fops;

    ret = misc_register(&pn544_dev->pn544_device);
    if (ret) {
        printk(KERN_INFO "%s : misc_register failed\n", __FILE__);
        goto err_misc_register;
    }

    /* request irq.  the irq is set whenever the chip has data available
    * for reading.  it is cleared when all data has been read.
    */
    printk(KERN_INFO "%s : requesting IRQ %d\n", __func__, client->irq);
    pn544_dev->irq_enabled = true;
    ret = request_irq(client->irq, pn544_dev_irq_handler,
    IRQF_TRIGGER_HIGH, client->name, pn544_dev);
    if (ret) {
        dev_err(&client->dev, "request_irq failed\n");
        goto err_request_irq_failed;
    }

    pn544_disable_irq(pn544_dev);
#ifdef _PN544_DEV_ERROR_FIX
    g_pn544_dev = pn544_dev;
#endif
    i2c_set_clientdata(client, pn544_dev);
#ifdef _PN544_I2C_TEST
    pn544_dev_test(client);
#endif
    return 0;

err_request_irq_failed:
    misc_deregister(&pn544_dev->pn544_device);
err_misc_register:
    mutex_destroy(&pn544_dev->read_mutex);
    kfree(pn544_dev);
err_exit:
    gpio_free(platform_data->firm_gpio);
#if 0    //kkong110316
err_firm:
    gpio_free(platform_data->ven_gpio);
err_ven:
    gpio_free(platform_data->irq_gpio);
#endif
    return ret;
}

static int pn544_remove(struct i2c_client *client)
{
    struct pn544_dev *pn544_dev;

    pn544_dev = i2c_get_clientdata(client);
    free_irq(client->irq, pn544_dev);
    misc_deregister(&pn544_dev->pn544_device);
    mutex_destroy(&pn544_dev->read_mutex);
    gpio_free(pn544_dev->irq_gpio);
    gpio_free(pn544_dev->ven_gpio);
    gpio_free(pn544_dev->firm_gpio);
    kfree(pn544_dev);

    return 0;
}

static int pn544_suspend(struct i2c_client *client, pm_message_t mesg)
{
#ifdef _PN544_I2C_TEST
    printk(KERN_ERR "%s :\n", __func__);
#elif defined(_PN544_I2C_LOG_VIEW)
    if(log_enable)
        printk(KERN_ERR "%s :\n", __func__);
#endif
    //platform_data->set_power(&client->dev,0);

    return 0;
}

static int pn544_resume(struct i2c_client *client)
{
#ifdef _PN544_I2C_TEST
    printk(KERN_ERR "%s :\n", __func__);
#elif defined(_PN544_I2C_LOG_VIEW)
    if(log_enable)
        printk(KERN_ERR "%s :\n", __func__);
#endif
    //platform_data->set_power(&client->dev,1);

    return 0;
}

static const struct i2c_device_id pn544_id[] = {
	{ "pn544", 0 },
	{ }
};

static struct i2c_driver pn544_driver = {
    .driver		= {
                        .name	= "pn544",
     },
    .probe		= pn544_probe,                        
    .remove		= pn544_remove,
    .suspend    = pn544_suspend,
    .resume     = pn544_resume,
    .id_table	= pn544_id,    
};

/*
* module load/unload record keeping
*/

static int __init pn544_dev_init(void)
{
    int ret;

    //printk(KERN_INFO "Loading pn544 driver\n");
    ret = i2c_add_driver(&pn544_driver);
    printk(KERN_INFO"%s: i2c_add_driver result %d\n", __func__, ret);

    return ret;
}
module_init(pn544_dev_init);

static void __exit pn544_dev_exit(void)
{
    printk(KERN_INFO "Unloading pn544 driver\n");
    i2c_del_driver(&pn544_driver);
}
module_exit(pn544_dev_exit);

MODULE_AUTHOR("Sylvain Fonteneau");
MODULE_DESCRIPTION("NFC PN544 driver");
MODULE_LICENSE("GPL");
