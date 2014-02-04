/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_spi.c
 
 Description : fc8050 host interface
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
*******************************************************************************/
#include <linux/input.h>
#include <linux/spi/spi.h>

#include "fci_types.h"
#include "fc8050_regs.h"
#include "fci_oal.h"

#define DRIVER_NAME "fc8050_spi"

#define HPIC_READ			0x01	// read command
#define HPIC_WRITE			0x02	// write command
#define HPIC_AINC			0x04	// address increment
#define HPIC_BMODE			0x00	// byte mode
#define HPIC_WMODE          0x10	// word mode
#define HPIC_LMODE          0x20	// long mode
#define HPIC_ENDIAN			0x00	// little endian
#define HPIC_CLEAR			0x80	// currently not used

#define CHIPID 0
#if (CHIPID == 0)
#define SPI_CMD_WRITE                           0x0
#define SPI_CMD_READ                            0x1
#define SPI_CMD_BURST_WRITE                     0x2
#define SPI_CMD_BURST_READ                      0x3
#else
#define SPI_CMD_WRITE                           0x4
#define SPI_CMD_READ                            0x5
#define SPI_CMD_BURST_WRITE                     0x6
#define SPI_CMD_BURST_READ                      0x7
#endif 

struct spi_device *fc8050_spi = NULL;

// 2011.11.28 vincent
static u8 tx_data[10] ____cacheline_aligned;// org 8
static u8 data_buf[8192+4] ____cacheline_aligned;//org 8192
static u8 rdata_buf[8192+4] ____cacheline_aligned;//org 8192

static DEFINE_MUTEX(lock);

// kuzuri 2011.12.02 - bulk write need not rx...
int fc8050_spi_write_by_bulk(struct spi_device *spi, u8 *txbuf, u16 tx_length)
{
	s32 res;
	
	struct spi_message	message;
	struct spi_transfer	x;

	spi_setup(spi);

	spi_message_init(&message);
	memset(&x, 0, sizeof x);

	spi_message_add_tail(&x, &message);
	
	//memcpy(data_buf, txbuf, tx_length);
	
	x.tx_buf = txbuf;
	x.rx_buf = rdata_buf;
	x.len = tx_length;
	
	res = spi_sync(spi, &message);

	return res;
}

int fc8050_spi_write_then_read(struct spi_device *spi, u8 *txbuf, u16 tx_length, u8 *rxbuf, u16 rx_length)
{
	s32 res;
	
	struct spi_message	message;
	struct spi_transfer	x;

  if ( rxbuf==NULL )
    PRINTF(0,"fc8050_spi_write_then_read():: rxbuf addr is NULL... rx length= %d\n", rx_length);

  //printk("FC8050_spi_write_then_read():: spi.max_speed = %d\n", spi->max_speed_hz );

	spi_setup(spi);

	spi_message_init(&message);
	memset(&x, 0, sizeof x);

	spi_message_add_tail(&x, &message);
	
	memcpy(data_buf, txbuf, tx_length);
	
	x.tx_buf = data_buf;
	x.rx_buf = rdata_buf;
	x.len = tx_length + rx_length;
	
	res = spi_sync(spi, &message);

  if ( rxbuf!=NULL )
	memcpy(rxbuf, x.rx_buf + tx_length, rx_length);

	return res;
}

int fc8050_spi_write_then_read_burst(struct spi_device *spi, u8 *txbuf, u16 tx_length, u8 *rxbuf, u16 rx_length)
{
  int res;

  struct spi_message	message;
  struct spi_transfer	x;

  //TDMB_MSG_FCI_BB("[%s]spi=%x\n", __func__, (unsigned int)spi);

  spi_message_init(&message);
  memset(&x, 0, sizeof x);

  spi_message_add_tail(&x, &message);

  x.tx_buf = txbuf;
  x.rx_buf = rxbuf;
  x.len = tx_length + rx_length;

  res = spi_sync(spi, &message);

  return res;
}

static int spi_bulkread(HANDLE hDevice, u8 addr, u8 *data, u16 length)
{
	s32 ret;

	tx_data[0] = SPI_CMD_BURST_READ;
	tx_data[1] = addr;

	ret = fc8050_spi_write_then_read(fc8050_spi, &tx_data[0], 2, &data[0], length);

	if(ret)
	{
		PRINTF(0, "fc8050_spi_bulkread fail : %d\n", ret);
		return BBM_NOK;
	}

	return BBM_OK;
}

static int spi_bulkwrite(HANDLE hDevice, u8 addr, u8* data, u16 length)
{
	s32 ret;
	s32 i;
	
	tx_data[0] = SPI_CMD_BURST_WRITE;
	tx_data[1] = addr;

	for(i=0;i<length;i++)
	{
		tx_data[2+i] = data[i];
	}

	//PRINTF(0, "fc8050_spi_builkwrite():: addr= %X , data[0]= %X , length= %d\n", addr, data[0], length);

	//ret =fc8050_spi_write_then_read(fc8050_spi, &tx_data[0], length+2, 0, 0);
	ret = fc8050_spi_write_by_bulk(fc8050_spi, &tx_data[0], length+2);

	if(ret)
	{
		PRINTF(0, "fc8050_spi_bulkwrite fail : %d\n", ret);
		return BBM_NOK;
	}
	
	return BBM_OK;
}

static int spi_dataread(HANDLE hDevice, u8 addr, u8* data, u16 length)
{
	s32 ret=0;

	tx_data[0] = SPI_CMD_BURST_READ;
	tx_data[1] = addr;

	//ret = fc8050_spi_write_then_read(fc8050_spi, &tx_data[0], 2, &data[0], length);
	ret = fc8050_spi_write_then_read_burst(fc8050_spi, &tx_data[0], 2, &data[0], length);
	if(ret)
	{
		PRINTF(0, "fc8050_spi_dataread fail : %d\n", ret);
		return BBM_NOK;
	}

	return BBM_OK;
}

#if 0
static int __devinit fc8050_spi_probe(struct spi_device *spi)
{
	s32 ret;
	
	PRINTF(0, "fc8050_spi_probe\n");

	//spi->max_speed_hz =  4000000;
	spi->max_speed_hz =  8000000;
	//spi->max_speed_hz =  5400000; // kuzuri - not use this.  Real value is in DMBDRV_KTTECH.c
	printk("FC8050_SPI_PROBE()>>>>>>> :: spi.max_speed= %d\n", spi->max_speed_hz);
	
	ret = spi_setup(spi);
	if (ret < 0)
		return ret;

	fc8050_spi = spi;

	return ret;
}

static int fc8050_spi_remove(struct spi_device *spi)
{

	return 0;
}
#endif

/*
static struct spi_driver fc8050_spi_driver = {
	.driver = {
		.name		= DRIVER_NAME,
		.owner		= THIS_MODULE,
	},
	.probe		= fc8050_spi_probe,
	.remove		= __devexit_p(fc8050_spi_remove),
};
*/

int fc8050_spi_init(HANDLE hDevice, u16 param1, u16 param2)
{
//	int res;

	printk("FC8050_SPI_INIT():::\n");
#if 0
	res = spi_register_driver(&fc8050_spi_driver);
	
	if(res)
	{
		PRINTF(0, "fc8050_spi register fail : %d\n", res);
		return BBM_NOK;
	}
#endif
	return BBM_OK;
}

int fc8050_spi_byteread(HANDLE hDevice, u16 addr, u8 *data)
{
	int res;
	u8 command = HPIC_READ | HPIC_BMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkread(hDevice, BBM_DATA_REG, data, 1);
	mutex_unlock(&lock);

	return res;
}

int fc8050_spi_wordread(HANDLE hDevice, u16 addr, u16 *data)
{
	int res;
	u8 command = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = HPIC_READ | HPIC_WMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkread(hDevice, BBM_DATA_REG, (u8*)data, 2);
	mutex_unlock(&lock);
	
	return res;
}

int fc8050_spi_longread(HANDLE hDevice, u16 addr, u32 *data)
{
	int res;
	u8 command = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkread(hDevice, BBM_DATA_REG, (u8*)data, 4);
	mutex_unlock(&lock);

	return res;
}

int fc8050_spi_bulkread(HANDLE hDevice, u16 addr, u8 *data, u16 length)
{
	int res;
	u8 command = HPIC_READ | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkread(hDevice, BBM_DATA_REG, data, length);
	mutex_unlock(&lock);
	
	return res;
}

int fc8050_spi_bytewrite(HANDLE hDevice, u16 addr, u8 data)
{
	int res;
	u8 command = HPIC_WRITE | HPIC_BMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkwrite(hDevice, BBM_DATA_REG, (u8*)&data, 1);
	mutex_unlock(&lock);
	
	return res;
}

int fc8050_spi_wordwrite(HANDLE hDevice, u16 addr, u16 data)
{
	int res;
	u8 command = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	if(BBM_SCI_DATA <= addr && BBM_SCI_SYNCRX >= addr)
		command = HPIC_WRITE | HPIC_WMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkwrite(hDevice, BBM_DATA_REG, (u8*)&data, 2);
	mutex_unlock(&lock);
	
	return res;
}

int fc8050_spi_longwrite(HANDLE hDevice, u16 addr, u32 data)
{
	int res;
	u8 command = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkwrite(hDevice, BBM_DATA_REG, (u8*)&data, 4);
	mutex_unlock(&lock);
	
	return res;
}

int fc8050_spi_bulkwrite(HANDLE hDevice, u16 addr, u8* data, u16 length)
{
	int res;
	u8 command = HPIC_WRITE | HPIC_AINC | HPIC_BMODE | HPIC_ENDIAN;

	mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_bulkwrite(hDevice, BBM_DATA_REG, data, length);
	mutex_unlock(&lock);
	
	return res;
}

int fc8050_spi_dataread(HANDLE hDevice, u16 addr, u8* data, u16 length)
{
	int res;
	u8 command = HPIC_READ | HPIC_BMODE | HPIC_ENDIAN;

    mutex_lock(&lock);
	res  = spi_bulkwrite(hDevice, BBM_COMMAND_REG, &command, 1);
	res |= spi_bulkwrite(hDevice, BBM_ADDRESS_REG, (u8*)&addr, 2);
	res |= spi_dataread(hDevice, BBM_DATA_REG, data, length);
	mutex_unlock(&lock);
	
	return res;
}

int fc8050_spi_deinit(HANDLE hDevice)
{
	PRINTF(NULL, "fc8050_spi_deinit\n");
	//spi_unregister_driver(&fc8050_spi_driver);
	return BBM_OK;
}
