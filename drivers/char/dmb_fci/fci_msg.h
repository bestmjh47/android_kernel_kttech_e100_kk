/*****************************************************************************
 Copyright(c) 2009 FCI Inc. All Rights Reserved
 
 File name : fc8050_isr.c
 
 Description : API of dmb baseband module
 
 History : 
 ----------------------------------------------------------------------
 2009/09/11 	jason		initial
*******************************************************************************/

#ifndef __FCI_MSG__
#define __FCI_MSG__

#ifdef __cplusplus
extern "C" {
#endif

#ifndef KTTECH_FINAL_BUILD
#define CHECK_RET(x) \
  if ( ret_val!=0 ) \
  {\
  DEVLOG("FC8050_ISR ___ ERROR :: (%d) \n", x);\
  }
#define DEVLOG(fmt, ...) printk(KERN_DEBUG "### <DMB>" fmt ,##__VA_ARGS__)  
#define DPRINTK(x...) printk("TDMB " x)
#else  
#define CHECK_RET(fmt, ...)
#define DEVLOG(fmt, ...)
#define DPRINTK(fmt, ...)
#endif

#ifdef __cplusplus
}
#endif

#endif // __FCI_MSG__

