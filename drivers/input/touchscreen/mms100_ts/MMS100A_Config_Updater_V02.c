/*
 * MMS100A Config Updater의 알고리듬이 구현된 부분입니다.
 * !!주의!!
 * 내용을 수정하시면 안됩니다.
 */
#include "MMS100A_Config_Updater_Customize.h"
#include "BOOT_120529_v28.c"
#include "CORE_120529_v28.c"
#include "PRIV_120529_v28.c"
#include "PUBL_120529_v28.c"

#define MFS_HEADER_		3
#define MFS_DATA_		1024
#define MFS_TAIL_		2

#define PACKET_			(MFS_HEADER_ + MFS_DATA_ + MFS_TAIL_)

typedef enum
{
	MSEC_NONE = -1,
	MSEC_BOOTLOADER = 0,
	MSEC_CORE,
	MSEC_PRIVATE_CONFIG,
	MSEC_PUBLIC_CONFIG,
	MSEC_LIMIT
} eMFSSection_t;

/*
 * State Registers
 */
#define ISC_ADDR_VERSION						0xE1
#define ISC_ADDR_SECTION_PAGE_INFO				0xE5

/*
 * Config Update Commands
 */
#define ISC_CMD_ENTER_ISC						0x5F
#define ISC_CMD_ENTER_ISC_PARA1					0x01
#define ISC_CMD_UPDATE_MODE						0xAE
#define ISC_SUBCMD_ENTER_UPDATE					0x55
#define ISC_SUBCMD_DATA_WRITE					0XF1
#define ISC_SUBCMD_LEAVE_UPDATE_PARA1			0x0F
#define ISC_SUBCMD_LEAVE_UPDATE_PARA2			0xF0
#define ISC_CMD_CONFIRM_STATUS					0xAF
#define ISC_STATUS_UPDATE_MODE					0x01
#define ISC_STATUS_CRC_CHECK_SUCCESS			0x03

//typedef int

#if 0
#define MFS_CHAR_2_BCD(num)	\
	(((num/10)<<4) + (num%10))
#else
#define MFS_CHAR_2_BCD(num) num
#endif
#define MFS_MAX(x, y)		( ((x) > (y))? (x) : (y) )

typedef struct
{
	unsigned char version;
	unsigned char compatible_version;
	unsigned char start_addr;
	unsigned char end_addr;
} tMFSFirmInfo_t;

#define MFS_DEFAULT_SLAVE_ADDR	0x48

#define SECTION_NAME_		5
//static const char section_name[MFS_SECTION_][SECTION_NAME_] =
//{ "BOOT", "CORE", "PRIV", "PUBL" };
static const struct firmware_data  *fw_data[MFS_SECTION_];

static const unsigned char crc0buf[31] =
{ 0x1D, 0x2C, 0x05, 0x34, 0x95, 0xA4, 0x8D, 0xBC, 0x59, 0x68, 0x41, 0x70, 0xD1,
		0xE0, 0xC9, 0xF8, 0x3F, 0x0E, 0x27, 0x16, 0xB7, 0x86, 0xAF, 0x9E, 0x7B,
		0x4A, 0x63, 0x52, 0xF3, 0xC2, 0xEB };

static const unsigned char crc1buf[31] =
{ 0x1E, 0x9C, 0xDF, 0x5D, 0x76, 0xF4, 0xB7, 0x35, 0x2A, 0xA8, 0xEB, 0x69, 0x42,
		0xC0, 0x83, 0x01, 0x04, 0x86, 0xC5, 0x47, 0x6C, 0xEE, 0xAD, 0x2F, 0x30,
		0xB2, 0xF1, 0x73, 0x58, 0xDA, 0x99 };

static tMFSFirmInfo_t new_info[MFS_SECTION_];
static tMFSFirmInfo_t old_info[MFS_SECTION_];
static unsigned char *buf;

static mfs_bool_t is_config_recovery_mode(void);
static eMFSRet_t read_old_firm_info(void);
static eMFSRet_t check_compatibility(mfs_bool_t update[MFS_SECTION_],
		unsigned char compatible_version[MFS_SECTION_]);
static eMFSRet_t enter_ISC_mode(void);
static eMFSRet_t enter_config_update_mode(void);
static void check_renewed_section(mfs_bool_t update[MFS_SECTION_]);
static eMFSRet_t clear_validate_markers(const mfs_bool_t update[MFS_SECTION_]);
// static eMFSRet_t exit_config_update_mode(void);
static int update_sections(mfs_bool_t update[MFS_SECTION_]);
static eMFSRet_t __clear_page(unsigned char _page_addr);
static eMFSRet_t open_mbin(void);
static eMFSRet_t read_new_firm_info(void);
static eMFSRet_t close_mbin(void);

int MFS_config_update(unsigned char _slave_addr)
{
	eMFSRet_t ret;
//	int i,j;

	unsigned char compatible_version[MFS_SECTION_];
	mfs_bool_t update[MFS_SECTION_] =
	{ 0, };

	printk("namjja : %s\n", __func__);

	buf =  kmalloc(PACKET_, GFP_KERNEL);

	/*Config 모드 진입 전에는 설정된 slave address를 사용.*/
	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);

	/* I2C 통신을 시도하여 실패하면 config recovery mode로 보아 slave address를 default value로 변경 */
	if (is_config_recovery_mode())
		MFS_I2C_set_slave_addr(MFS_DEFAULT_SLAVE_ADDR);
	//old firmware version check

	if ((ret = read_old_firm_info()) && ret != MRET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

//	open_mbin(fh, dev);
	if ((ret = open_mbin()) && ret != MRET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	if ((ret = read_new_firm_info()) && ret != MRET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	check_renewed_section(update);

	if ((ret = check_compatibility(update, compatible_version))
			&& ret != MRET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	if ((ret = enter_ISC_mode()) && ret != MRET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	/*Config 모드 진입 후 slave address를 default value로 변경.*/
	MFS_I2C_set_slave_addr(MFS_DEFAULT_SLAVE_ADDR);

	if ((ret = enter_config_update_mode()) && ret != MRET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	/* Clear last page of each section. */
	if ((ret = clear_validate_markers(update)) && ret != MRET_SUCCESS)
		goto MCSDL_DOWNLOAD_FINISH;

	if ((ret = update_sections(update)) && ret != MRET_SUCCESS)
	{
		if ((ret = clear_validate_markers(update)) && ret != MRET_SUCCESS)
			goto MCSDL_DOWNLOAD_FINISH;
	}

//	if ((ret = exit_config_update_mode()) && ret != MRET_SUCCESS)
//		goto MCSDL_DOWNLOAD_FINISH;

	MFS_debug_msg("FIRMWARE_UPDATE_FINISHED!!!\n\n", 0, 0, 0);

	MCSDL_DOWNLOAD_FINISH:

	kfree(buf);

	close_mbin();

	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);

	// Reboot 해야 합니다!!!
	melfas_power(0);
	MFS_ms_delay(1000);
	melfas_power(1);
	return ret;
}

static mfs_bool_t is_config_recovery_mode(void)
{
	unsigned char rb;
	if (!MFS_I2C_read_with_addr(&rb, ISC_ADDR_VERSION, 1))
		return MFS_TRUE; //Fail 시 recovery mode로 인식.
	return MFS_FALSE;
}

static void check_renewed_section(mfs_bool_t update[MFS_SECTION_])
{
	int i;
	for (i = 0; i < MFS_SECTION_; i++)
	{
		MFS_debug_msg("<MELFAS> Section %d %d %d.\n", i, new_info[i].version, old_info[i].version);
		if (new_info[i].version == 0
				|| new_info[i].version != old_info[i].version)
		{
			update[i] = MFS_TRUE;
			MFS_debug_msg("<MELFAS> Section %d will be updated.\n", i, 0, 0);
		}
	}
}

eMFSRet_t MFS_config_validate(void)
{
	int i;
	eMFSRet_t ret;
	//TODO: 설정된 I2C slave address로 version을 읽어서 정상 다운로드 여부를 확인!
	MFS_I2C_set_slave_addr(mfs_i2c_slave_addr);

	if ((ret = read_old_firm_info()) && ret != MRET_SUCCESS)
		return ret;

	for (i = 0; i < MFS_SECTION_; i++)
		if (old_info[i].version != new_info[i].version)
			return MRET_VALIDATION_ERROR;

	return MRET_SUCCESS;
}

eMFSRet_t read_old_firm_info(void)
{
	int i;
	unsigned char readbuf[8];

	if (!MFS_I2C_read_with_addr(readbuf, ISC_ADDR_VERSION, 4))
		return MRET_I2C_ERROR;

	MFS_debug_msg("VER INFO READ\n", 0, 0, 0);
	for (i = 0; i < MFS_SECTION_; i++)
	{
		old_info[i].version = readbuf[i];
		MFS_debug_msg("\tSection %d: 0x%02X\n", i, old_info[i].version, 0);
	}
	old_info[MSEC_CORE].compatible_version = old_info[MSEC_BOOTLOADER].version;
	old_info[MSEC_PRIVATE_CONFIG].compatible_version =
			old_info[MSEC_PUBLIC_CONFIG].compatible_version =
					old_info[MSEC_CORE].version;

	if (!MFS_I2C_read_with_addr(readbuf, ISC_ADDR_SECTION_PAGE_INFO, 8))
		return MRET_I2C_ERROR;

	for (i = MFS_SECTION_; i--;)
	{
		old_info[i].start_addr = readbuf[i];
//		MFS_debug_msg("Start Page Addr of Section %d: %d\n", i,
//				old_info[i].start_addr, 0); // for debug
		old_info[i].end_addr = readbuf[i + MFS_SECTION_];
		MFS_debug_msg("End Page   Addr of Section %d: %d\n", i,
				old_info[i].end_addr, 0);
	}
	return MRET_SUCCESS;
}

eMFSRet_t check_compatibility(mfs_bool_t update[MFS_SECTION_],
		unsigned char expected_compat_version[MFS_SECTION_])
{
//#if 0 // namjja disable
#if 1
	int i, nRet = MRET_SUCCESS;
	// check the compatible version
	if (update[MSEC_BOOTLOADER])
		expected_compat_version[MSEC_CORE] = new_info[MSEC_BOOTLOADER].version;
	else
		expected_compat_version[MSEC_CORE] = old_info[MSEC_BOOTLOADER].version;

	if (update[MSEC_CORE])
		expected_compat_version[MSEC_PUBLIC_CONFIG] =
				expected_compat_version[MSEC_PRIVATE_CONFIG] =
						new_info[MSEC_CORE].version;
	else
		expected_compat_version[MSEC_PUBLIC_CONFIG] =
				expected_compat_version[MSEC_PRIVATE_CONFIG] =
						old_info[MSEC_CORE].version;

	//compare the compatible version
	for (i = MSEC_CORE; i <= MSEC_PUBLIC_CONFIG; i++)
	{
		if (update[i]
				&& expected_compat_version[i] != new_info[i].compatible_version)
		{
			MFS_debug_msg("<MELFAS> expected_compat_version[%d] = 0x%2X\n", i,
					expected_compat_version[i], 0);
			MFS_debug_msg("<MELFAS>	new_info[%d].compatible_version = 0x%2X\n",
					i, new_info[i].compatible_version, 0);
			MFS_debug_msg("<MELFAS> Fimware Compatible Version error!!!\n", 0,
					0, 0);
			nRet = MRET_COMPATIVILITY_ERROR;
		}
		else if (!update[i]
				&& expected_compat_version[i] != old_info[i].compatible_version)
		{
			MFS_debug_msg("<MELFAS> expected_compat_version[%d] = 0x%2X\n", i,
					expected_compat_version[i], 0);
			MFS_debug_msg("<MELFAS>	old_info[%d].compatible_version = 0x%2X\n",
					i, old_info[i].compatible_version, 0);
			MFS_debug_msg("<MELFAS> Fimware Compatible Version error!!!\n", 0,
					0, 0);
			nRet = MRET_COMPATIVILITY_ERROR;
		}
	}

	return nRet;
#else
	return MRET_SUCCESS;
#endif

}

eMFSRet_t enter_ISC_mode(void)
{
	unsigned char write_buffer[2];
	MFS_debug_msg("ENTER_ISC_MODE\n", 0, 0, 0);
	write_buffer[0] = ISC_CMD_ENTER_ISC; // command
	write_buffer[1] = ISC_CMD_ENTER_ISC_PARA1; // sub_command
	if (write_buffer[0] != ISC_CMD_ENTER_ISC
			|| write_buffer[1] != ISC_CMD_ENTER_ISC_PARA1)
	{
		MFS_debug_msg("ISC write buffer error!!!\n", 0, 0, 0);
		return MRET_WRITE_BUFFER_ERROR;
	}
	if (!MFS_I2C_write(write_buffer, 2))
	{
		MFS_debug_msg("ISC mode enter failed!!!\n", 0, 0, 0);
		return MRET_I2C_ERROR;
	}
	MFS_ms_delay(50);
	return MRET_SUCCESS;
}

static eMFSRet_t enter_config_update_mode(void)
{
#define UPDATE_MODE_ENTER_CMD_		10
	uint8_t update_mode_enter_cmd[UPDATE_MODE_ENTER_CMD_] =
	{ ISC_CMD_UPDATE_MODE, ISC_SUBCMD_ENTER_UPDATE, 0, 0, 0, 0, 0, 0, 0, 0 };
	unsigned char readbuf;
	int i;

	MFS_debug_msg("FIRMWARE_UPDATE_MODE_ENTER\n\n", 0, 0, 0);

	if (update_mode_enter_cmd[0] != ISC_CMD_UPDATE_MODE
			|| update_mode_enter_cmd[1] != ISC_SUBCMD_ENTER_UPDATE)
	{
		MFS_debug_msg("ISC write buffer error!!!\n", 0, 0, 0);
		return MRET_WRITE_BUFFER_ERROR;
	}
	for (i = 2; i < UPDATE_MODE_ENTER_CMD_; i++)
	{
		if (update_mode_enter_cmd[i] != 0)
		{
			MFS_debug_msg("ISC write buffer error!!!\n", 0, 0, 0);
			return MRET_WRITE_BUFFER_ERROR;
		}
	}

	if (!MFS_I2C_write(update_mode_enter_cmd, UPDATE_MODE_ENTER_CMD_))
	{
		MFS_debug_msg("Update mode enter failed!!!\n", 0, 0, 0);
		return MRET_I2C_ERROR;
	}

	readbuf = 0;

	while (!readbuf)
	{
		if (!MFS_I2C_read_with_addr(&readbuf, ISC_CMD_CONFIRM_STATUS, 1))
			return MRET_I2C_ERROR;
		// delay  1ms
		if (readbuf == ISC_STATUS_UPDATE_MODE)
			MFS_debug_msg("Firmware update mode enter success!!!\n", 0, 0, 0);
		else
		{
			MFS_debug_msg("Error detected!! status: 0x%02x.\n", readbuf, 0, 0);
			MFS_ms_delay(1000);
		}
	}

	return MRET_SUCCESS;
}

static eMFSRet_t clear_validate_markers(const mfs_bool_t update[MFS_SECTION_])
{
	eMFSRet_t ret = MRET_SUCCESS;
	int i, j;
	for (i = MSEC_CORE; i <= MSEC_PUBLIC_CONFIG; i++)
	{
#if 1
		if (update[i] && old_info[i].end_addr <= 30)
		{
//			MFS_debug_msg("old clear section[%d]: %d.\n", i, old_info[i].end_addr, 0); // for debug
			ret = (eMFSRet_t) __clear_page((unsigned char)old_info[i].end_addr);
			if (ret != MRET_SUCCESS)
				goto ERROR_HANDLER;
		}
#endif
	}
	for (i = MSEC_CORE; i <= MSEC_PUBLIC_CONFIG; i++)
	{
		if (update[i])
		{
			mfs_bool_t same_addr = MFS_FALSE;
			for (j = MSEC_CORE; j <= MSEC_PUBLIC_CONFIG; j++)
				if (new_info[j].end_addr == old_info[i].end_addr)
				{
					same_addr = MFS_TRUE;
					break;
				}
			/*
			 * old info의 marker section 외에 추가된 marker section을 erase.
			 */
			if (!same_addr)
			{
				ret = (eMFSRet_t) __clear_page((unsigned char)new_info[i].end_addr);
				if (ret != MRET_SUCCESS)
					goto ERROR_HANDLER;
			}
		}
	}
	ERROR_HANDLER: return ret;
}

static int update_sections(mfs_bool_t update[MFS_SECTION_])
{
	int i;
	unsigned char addr, readbuf;
	int page = 0;
	unsigned char *buffer;

	for (i = 0; i < MFS_SECTION_; i++)
	{
		if (update[i])
		{
			MFS_debug_msg("Section %d update start...\n", i, 0, 0);

			for (addr = new_info[i].start_addr; addr <= new_info[i].end_addr;
					addr++)
			{
				page = addr - new_info[i].start_addr;
				buffer = (unsigned char *)&fw_data[i]->data[page*PACKET_];
				//buffer = (unsigned char *)fw_data[i].data;
				//buffer = (unsigned char *)melfas_BOOT.data;

//				printk("namjja : page : %d\n", page*PACKET_);

				MFS_debug_msg("%dth page update start...\n", addr, 0, 0);

				if (buffer[0] != ISC_CMD_UPDATE_MODE
						|| buffer[1] != ISC_SUBCMD_DATA_WRITE || buffer[2] != addr)
				{
					//MFS_debug_msg("ISC write buffer error!!!\n", 0, 0, 0);
					MFS_debug_msg("ISC write buffer error!!! [%x][%x][%x]\n", buffer[0], buffer[1], buffer[2]);
					return MRET_WRITE_BUFFER_ERROR;
				}

				if (!MFS_I2C_write(buffer, PACKET_))
//				if (!MFS_I2C_write(buffer, MELFAS_BOOT_nLength_2))
					return MRET_I2C_ERROR;

				if (!MFS_I2C_read_with_addr(&readbuf, ISC_CMD_CONFIRM_STATUS,
						1))
					return MRET_I2C_ERROR;

				if (readbuf == ISC_STATUS_CRC_CHECK_SUCCESS)
				{
					MFS_debug_msg("Page update succeeded.\n", addr, 0, 0);
				}
				else
				{
					MFS_debug_msg("Error: status is 0x%02x.\n", readbuf, 0, 0);
					return MRET_CRC_ERROR;
				}
			}
			MFS_debug_msg("Update succeeded.\n", 0, 0, 0);
			update[i] = MFS_FALSE;
		}
	}

	return MRET_SUCCESS;
}
#if 0
eMFSRet_t exit_config_update_mode(void)
{
	unsigned char write_buffer[3];

	MFS_debug_msg("LEAVE_FIRMWARE_UPDATE_MODE\n\n", 0, 0, 0);

	write_buffer[0] = ISC_CMD_UPDATE_MODE; // command
	write_buffer[1] = ISC_SUBCMD_LEAVE_UPDATE_PARA1;
	write_buffer[2] = ISC_SUBCMD_LEAVE_UPDATE_PARA2;
	if (!MFS_I2C_write(write_buffer, 3))
	return MRET_I2C_ERROR;//delay 5ms
	MFS_ms_delay(5);
	return MRET_SUCCESS;
}
#endif

static eMFSRet_t __clear_page(unsigned char _page_addr)
{
	unsigned char readbuf;
	unsigned char crc0 = crc0buf[_page_addr];
	unsigned char crc1 = crc1buf[_page_addr];

//	printk("namjja : %s\n", __func__);
	memset(buf, 0xFF, PACKET_);

	buf[0] = ISC_CMD_UPDATE_MODE; // command
	buf[1] = ISC_SUBCMD_DATA_WRITE; // sub_command
	buf[2] = _page_addr;
	buf[MFS_HEADER_ + MFS_DATA_ + 0] = crc0;
	buf[MFS_HEADER_ + MFS_DATA_ + 1] = crc1;

	MFS_debug_msg("%dth page clear start...", _page_addr, 0, 0);

	if (buf[0] != ISC_CMD_UPDATE_MODE || buf[1] != ISC_SUBCMD_DATA_WRITE
			|| buf[2] != _page_addr)
	{
		MFS_debug_msg("ISC write buffer error!!!\n", 0, 0, 0);
		return MRET_WRITE_BUFFER_ERROR;
	}

	if (!MFS_I2C_write(buf, PACKET_))
		return MRET_I2C_ERROR;
	if (!MFS_I2C_read_with_addr(&readbuf, ISC_CMD_CONFIRM_STATUS, 1))
		return MRET_I2C_ERROR;

	if (readbuf != ISC_STATUS_CRC_CHECK_SUCCESS)
	{
		MFS_debug_msg("Page clear failed!! Status: 0x%2X\n", readbuf, 0, 0);
		return MRET_CRC_ERROR;
	}

	MFS_debug_msg("Page clear succeeded!!\n", 0, 0, 0);
	return MRET_SUCCESS;

}

static eMFSRet_t open_mbin(void)
{
	fw_data[0] = &melfas_BOOT;
	fw_data[1] = &melfas_CORE;
	fw_data[2] = &melfas_PRIV;
	fw_data[3] = &melfas_PUBL;

	return MRET_SUCCESS;
}

static eMFSRet_t read_new_firm_info(void)
{
	eMFSRet_t ret = MRET_SUCCESS;
//	int i, j;
	int i;

	for (i = 0; i < MFS_SECTION_; i++)
	{
//		printk("namjja : SECTION_NAME [%s]\n", fw_data[i]->section_name);

		new_info[i].version = MFS_CHAR_2_BCD(fw_data[i]->section_version);
//		printk("namjja : VERSION [%d]\n", MFS_CHAR_2_BCD(fw_data[i]->section_version));	
		new_info[i].start_addr = fw_data[i]->start_page_addr;
//		printk("namjja : START_ADD [%d]\n", fw_data[i]->start_page_addr);

		new_info[i].end_addr = fw_data[i]->end_page_addr;
//		printk("namjja : END_PAGE_ADDR [%d]\n", fw_data[i]->end_page_addr);

		new_info[i].compatible_version = MFS_CHAR_2_BCD(fw_data[i]->compatible_version);
//namjja : disable debug msg
#if 0
		printk("namjja : COMPATIBLE_VERSION [%d]\n", fw_data[i]->compatible_version);
		printk("namjja : data ");
		for (j=0; j < 10; j++)
		{
			printk("[%x] ", fw_data[i]->data[j]);
		}
		printk("\n");
#endif
	}
	return ret;
}

eMFSRet_t close_mbin()
{
	return MRET_SUCCESS;
}

