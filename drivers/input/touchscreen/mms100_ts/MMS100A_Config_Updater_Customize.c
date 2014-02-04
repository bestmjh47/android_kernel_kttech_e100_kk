/*
 * MMS100A Config Updater�� customize�ϱ� ���� �ҽ��Դϴ�.
 * ������ ���� �����ϼž� �ϴ� �ҽ��Դϴ�.
 */
#include "MMS100A_Config_Updater_Customize.h"

/*
 *  TODO: ��ſ� ����ϴ� slave address�� ����� �ּ���.
 * !! ���Ǹ� ��Ź�帳�ϴ� !!
 * Config ���� �� ���Ǵ� default slave address�� 0x48 �Դϴ�.
 * �ش� slave address�� ����ϴ� I2C slave device�� ������ Ȯ���� �ֽð�,
 * ���� �ִٸ� MMM100A�� �ٿ�ε� �� ���� �ش� slave device�� disable ���� �ֽʽÿ�.
 */
const unsigned char mfs_i2c_slave_addr = 0x48;

/*
 * TODO: .mbin file�� ��θ� ������ �ּ���.
 * ������ ��� ǥ�ù���(e.g. slash)�� �Բ� �Է��� �ּ���.
 */
char* mfs_mbin_path = "./";

/*
 * TODO: .mbin�� ������ ��section�� filename�� ������ �ּ���.
 */

char section_filename[MFS_SECTION_][255] =
{ "BOOT", "CORE", "PRIV", "PUBL" };

/*���� setting�� slave address�� �˰� ���� �� �����ϼ���.*/
uint8_t mfs_slave_addr;

mfs_bool_t MFS_I2C_set_slave_addr(unsigned char _slave_addr)
{
//namjja : change download addr
//	mfs_slave_addr = _slave_addr << 1; /*�������� ���ʽÿ�.*/
	mfs_slave_addr = _slave_addr; /*�������� ���ʽÿ�.*/

	/* TODO: I2C slave address�� ������ �ּ���. */

	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_read_with_addr(unsigned char* _read_buf, unsigned char _addr,
		int _length)
{
	/* TODO: I2C�� 1 byte address�� �� �� _length ������ŭ �о� _read_buf�� ä�� �ּ���. */
	if (!melfas_write(mfs_slave_addr, &_addr, 1))
		return MFS_FALSE;
	if (!melfas_read(mfs_slave_addr, _read_buf, _length))
		return MFS_FALSE;

	return MFS_TRUE;
}

//mfs_bool_t MFS_I2C_write(const unsigned char* _write_buf, int _length)
mfs_bool_t MFS_I2C_write(unsigned char* _write_buf, int _length)
{
	/*
	 * TODO: I2C�� _write_buf�� ������ _length ������ŭ �� �ּ���.
	 * address�� ����ؾ� �ϴ� �������̽��� ���, _write_buf[0]�� address�� �ǰ�
	 * _write_buf+1���� _length-1���� �� �ֽø� �˴ϴ�.
	 */
	if (!melfas_write(mfs_slave_addr, _write_buf, _length))
		return MFS_FALSE;
	return MFS_TRUE;
}

void MFS_debug_msg(const char* fmt, int a, int b, int c)
{
	printk(fmt, a, b, c);
}
void MFS_ms_delay(int msec)
{
	udelay(msec*1000);
}
