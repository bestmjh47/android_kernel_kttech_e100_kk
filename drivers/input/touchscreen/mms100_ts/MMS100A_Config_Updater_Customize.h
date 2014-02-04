/*
 * MMS100A Config Updater�� customize�ϱ� ���� ����Դϴ�.
 * ������ ���� �����ϼž� �ϴ� ����Դϴ�.
 */
#ifndef __MMS100A_CONFIG_UPDATER_CUSTOMIZE_H__
#define __MMS100A_CONFIG_UPDATER_CUSTOMIZE_H__

/*
 * TODO: �ʿ��� header ������ include�� �ּ���.
 * �ʿ��� �������̽��� �Ʒ��� �����ϴ�.
 * memset, malloc, free, strcmp, strstr, ����� �޼����� ���� �Լ� ��.
 */
#if 0
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#endif
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/slab.h>
#include "melfas_ts.h"

#include "MMS100A_Config_Updater.h"

//#include "BOOT.c"
//#include "CORE.h"
//#include "PRIV.h"
//#include "PUBL.h"

/*
 * Boolean ���� type �� define.
 * �״�� �μŵ� �ǰ�, system�� �°� ���� �ּŵ� �˴ϴ�.
 */
typedef int mfs_bool_t;
#define MFS_TRUE		(0==0)
#define MFS_FALSE		(0!=0)


extern const unsigned char mfs_i2c_slave_addr;

extern char section_filename[MFS_SECTION_][255];
extern char* mfs_mbin_path;
extern mfs_bool_t MFS_I2C_set_slave_addr(unsigned char _slave_addr);
extern mfs_bool_t MFS_I2C_read_with_addr(unsigned char* _read_buf, unsigned char _addr, int _length);
extern mfs_bool_t MFS_I2C_write(unsigned char* _write_buf, int _length);
extern void MFS_debug_msg(const char* fmt, int a, int b, int c);
extern void MFS_ms_delay(int msec);
int melfas_read(uint8_t slave_addr, uint8_t* buffer, int packet_len);
int melfas_write(uint8_t slave_addr, uint8_t* buffer, int packet_len);
int melfas_power(int on);
#endif
