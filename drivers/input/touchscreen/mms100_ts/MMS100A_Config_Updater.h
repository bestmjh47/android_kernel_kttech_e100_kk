/*
 * MMS100A Config Updater의 public 헤더입니다.
 * !!주의!!
 * 내용을 수정하시면 안됩니다.
 */
#ifndef __MMS100A_CONFIG_UPDATER_H__
#define __MMS100A_CONFIG_UPDATER_H__

#define MFS_SECTION_	4

/*
 * Return values
 */
typedef enum
{
	MRET_NONE = -1,
	MRET_SUCCESS = 0,
	MRET_FILE_OPEN_ERROR,
	MRET_FILE_CLOSE_ERROR,
	MRET_FILE_FORMAT_ERROR,
	MRET_WRITE_BUFFER_ERROR,
	MRET_I2C_ERROR,
	MRET_CRC_ERROR,
	MRET_VALIDATION_ERROR,
	MRET_COMPATIVILITY_ERROR,
	MRET_LIMIT
} eMFSRet_t;

/*
 * Interfaces
 */
extern eMFSRet_t MFS_config_update(unsigned char slave_addr);
extern eMFSRet_t MFS_config_validate(void);

struct firmware_data {
	char *chip;
	char *section_name;
	char *compatible_section_name;
	int section_version;
	int start_page_addr;
	int end_page_addr;
	int compatible_version;
	int length;
	unsigned char data[];
};

#endif //__MMS100A_CONFIG_UPDATER_H__
