#ifndef __DMBDRVH__
#define __DMBDRVH__

#include "fci_types.h"
#include "fci_ringbuffer.h"
#include <linux/list.h>
#include "dmbdrv_extern.h"

#define TS_USE_WORK_QUEUE

typedef struct {
	HANDLE				*hInit;
	struct list_head		hList;
	struct fci_ringbuffer		RingBuffer;
	#ifdef TS_USE_WORK_QUEUE
	struct work_struct ts_event_work;
	#endif
	u8				*buf;
	u8				dmbtype;
} DMB_OPEN_INFO_T;

typedef struct {
	struct list_head		hHead;		
} DMB_INIT_INFO_T;

static u32 g_uiKOREnsembleFullFreq[MAX_KOREABAND_FULL_CHANNEL] = 
{
	175280,177008,178736,
	181280,183008,184736,
	187280,189008,190736,
	193280,195008,196736,
	199280,201008,202736,
	205280,207008,208736,
	211280,213008,214736
};

typedef struct _tagCHANNELDB_INFO
{
	unsigned short uiEnsembleID;
	unsigned char	ucSubchID;
	unsigned short uiStartAddress;
	unsigned char ucTMId;
	unsigned char ucServiceType;
	unsigned long ulServiceID;
	unsigned char NumberofUserAppl;
	unsigned short UserApplType[USER_APPL_NUM_MAX];
	unsigned char UserApplLength[USER_APPL_NUM_MAX];
	unsigned char UserApplData[USER_APPL_NUM_MAX][USER_APPL_DATA_SIZE_MAX];

} SubChInfoTypeDB;
#endif