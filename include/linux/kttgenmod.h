#ifndef __KTTMODH__
#define __KTTMODH__

typedef enum {
   DEVICE_SPI = 0x1,
} kttgenmod_device_t;

uint get_msmchip_id(void);
uint get_msmchip_ver(void);

void kttgenmod_set_device(kttgenmod_device_t device);
void kttgenmod_clear_device(kttgenmod_device_t device);

#endif

