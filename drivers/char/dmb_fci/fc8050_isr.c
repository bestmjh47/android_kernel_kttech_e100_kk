/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_isr.c
 
 Description : fc8050 interrupt service routine
 
 History : 
 ----------------------------------------------------------------------
 2009/08/29 	jason		initial
*******************************************************************************/
#include <linux/input.h>
#include <linux/spi/spi.h>

#include "fci_types.h"
#include "fci_hal.h"
#include "fc8050_regs.h"
#include "fci_msg.h"

// 2011.11.28 vincent
static u8 ficBuffer[4096]  ____cacheline_aligned;
static u8 mscBuffer[8192+4] ____cacheline_aligned;

//int (*pFicCallback)(HANDLE hDevice, u8 *data, int length) = NULL;
int (*pFicCallback)(void *, unsigned char *, int ) = NULL;
//int (*pMscCallback)(HANDLE hDevice, u8 subchid, u8 *data, int length) = NULL;
int (*pMscCallback)(void *, unsigned char, unsigned char *, int ) = NULL;

u32 gFicUserData;
u32 gMscUserData;

void fc8050_isr(HANDLE hDevice)
{
  u16 overrun;
  u16 buf_enable=0;   
  u8	extIntStatus = 0;
  int ret_val = 0;

  u16	mfIntStatus = 0;
  u16	size;
  int  	i;
  int mfInt_one = 0;

  //2011.11.18 - FCI Vincent, External Interrupt Disable
  //bbm_write(hDevice, BBM_COM_INT_ENABLE, 0); // kuzuri del 2011.11.21 - 화면 멈춤 현상(SPI.error) 발생!!!
  ret_val = bbm_read(hDevice, BBM_COM_INT_STATUS, &extIntStatus);
  bbm_write(hDevice, BBM_COM_INT_STATUS, extIntStatus);
  bbm_write(hDevice, BBM_COM_INT_STATUS, 0x00);

  //DEVLOG(KERN_ERR "FC8050_ISR()---- extIntStatus= %X\n", extIntStatus);

  if(extIntStatus & BBM_MF_INT)
  {
    ret_val = bbm_word_read(hDevice, BBM_BUF_STATUS, &mfIntStatus);
    CHECK_RET(1)
    bbm_word_write(hDevice, BBM_BUF_STATUS, mfIntStatus);
    bbm_word_write(hDevice, BBM_BUF_STATUS, 0x0000);

    if(mfIntStatus & 0x0100)
    {		  
      DEVLOG(KERN_ERR "FC8050_ISR()--- 3___ mfIntstatus 0x0100 \n");
      bbm_word_read(hDevice, BBM_BUF_FIC_THR, &size);
      size += 1;
      if(size-1)
      {
        ret_val = bbm_data(hDevice, BBM_COM_FIC_DATA, &ficBuffer[0], size);
        // kuzuri 2011.12.02 - bbm_data fail 일 경우, 아래 FicCallback 에서 Kernel panic 발생함. extIntStatus=1, mfIntStatus = 0x100 인 경우 문제됨.
        if ( ret_val!=0 )
        {
          DEVLOG(KERN_ERR "Error_____ FC8050_ISR()--- bbm_data read FAIL.. clear isr.\n");
          mfIntStatus = 0;
        }
        else
        {
          if(pFicCallback) 
            (*pFicCallback)(hDevice, &ficBuffer[2], size);  // kuzuri 2011.12.02 - spi_readburst 함수 수정.
            //(*pFicCallback)(hDevice, &ficBuffer[4], size);
        }
      } 
    }

    if ( mfIntStatus == 0 || mfIntStatus>1 )
      DEVLOG(KERN_ERR "FC8050_ISR()--- mfInt= %d  \n",mfIntStatus);

    if ( mfIntStatus==0x1 )
    {
      mfInt_one = 1;
    }

    // 2011.11.16 FCI Vincent
    if(mfIntStatus == 0)
    { 
      //buffer Clear
      overrun = 1;
      bbm_word_read(NULL, BBM_BUF_ENABLE, &buf_enable);
      buf_enable &= ~overrun;
      bbm_word_write(NULL, BBM_BUF_ENABLE, buf_enable);
      buf_enable |= overrun; 
      bbm_word_write(NULL, BBM_BUF_ENABLE, buf_enable);
      DEVLOG(KERN_ERR "FC8050_ISR()- buffer clear mfIntStatus = %X\n", mfIntStatus);
    }
    else
    {
      for(i=0; i<8; i++)
      {
        if(mfIntStatus & (1<<i))
        {
          ret_val = bbm_word_read(hDevice, BBM_BUF_CH0_THR+i*2, &size);
          CHECK_RET(2)

          size += 1;

          if(size-1)
          {
            u8  subChId;

            ret_val = bbm_read(hDevice, BBM_BUF_CH0_SUBCH+i, &subChId);
            subChId = subChId & 0x3f;

            ret_val = bbm_data(hDevice, (BBM_COM_CH0_DATA+i), &mscBuffer[0], size);
            CHECK_RET(3)

            if(pMscCallback)
              (*pMscCallback)(hDevice, subChId, &mscBuffer[2], size);  // kuzuri 2011.12.02 - spi_readburst 함수 수정.
              //(*pMscCallback)(hDevice, subChId, &mscBuffer[4], size);
          }
        }

        if ( mfInt_one==1 ) break;
      }
    }
  }

 
#if 0
	if(extIntStatus & BBM_SCI_INT) {
		extern void PL131_IntHandler(void);
		PL131_IntHandler();
					}

	if(extIntStatus & BBM_WAGC_INT) {
	}

	if(extIntStatus & BBM_RECFG_INT) {
				}

	if(extIntStatus & BBM_TII_INT) {
			}

	if(extIntStatus & BBM_SYNC_INT) {
		}

	if(extIntStatus & BBM_I2C_INT) {
	}
	
	if(extIntStatus & BBM_MP2_INT) {
	}
#endif
	//2011.11.18 - FCI Vincent External Interrupt Enable	
	//bbm_write(hDevice, BBM_COM_INT_ENABLE, BBM_MF_INT);  // kuzuri del 2011.11.21 - 화면 멈춤 현상(SPI.error) 발생!!!
}

