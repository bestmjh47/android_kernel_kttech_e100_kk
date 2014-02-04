/*
 * MMS100A Config Updater를 customize하기 위한 소스입니다.
 * 포팅을 위해 수정하셔야 하는 소스입니다.
 */
#include "MMS100A_Config_Updater_Customize.h"

/*
 *  TODO: 통신에 사용하는 slave address를 기록해 주세요.
 * !! 주의를 부탁드립니다 !!
 * Config 실패 시 사용되는 default slave address는 0x48 입니다.
 * 해당 slave address를 사용하는 I2C slave device가 없는지 확인해 주시고,
 * 만일 있다면 MMM100A를 다운로드 할 때는 해당 slave device를 disable 시켜 주십시오.
 */
const unsigned char mfs_i2c_slave_addr = 0x48;

/*
 * TODO: .mbin file의 경로를 설정해 주세요.
 * 마지막 경로 표시문자(e.g. slash)를 함께 입력해 주세요.
 */
char* mfs_mbin_path = "./";

/*
 * TODO: .mbin을 제외한 각section의 filename을 설정해 주세요.
 */

char section_filename[MFS_SECTION_][255] =
{ "BOOT", "CORE", "PRIV", "PUBL" };

/*현재 setting된 slave address를 알고 싶을 때 참조하세요.*/
uint8_t mfs_slave_addr;

mfs_bool_t MFS_I2C_set_slave_addr(unsigned char _slave_addr)
{
//namjja : change download addr
//	mfs_slave_addr = _slave_addr << 1; /*수정하지 마십시오.*/
	mfs_slave_addr = _slave_addr; /*수정하지 마십시오.*/

	/* TODO: I2C slave address를 셋팅해 주세요. */

	return MFS_TRUE;
}

mfs_bool_t MFS_I2C_read_with_addr(unsigned char* _read_buf, unsigned char _addr,
		int _length)
{
	/* TODO: I2C로 1 byte address를 쓴 후 _length 갯수만큼 읽어 _read_buf에 채워 주세요. */
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
	 * TODO: I2C로 _write_buf의 내용을 _length 갯수만큼 써 주세요.
	 * address를 명시해야 하는 인터페이스의 경우, _write_buf[0]이 address가 되고
	 * _write_buf+1부터 _length-1개를 써 주시면 됩니다.
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
