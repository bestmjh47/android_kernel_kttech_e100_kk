/****************************************************************************
 ****************************************************************************
 ***
 ***   This header was automatically generated from a Linux kernel header
 ***   of the same name, to make information necessary for userspace to
 ***   call into the kernel available to libc.  It contains only constants,
 ***   structures, and macros generated from the original header, and thus,
 ***   contains no copyrightable information.
 ***
 ****************************************************************************
 ****************************************************************************/
// KTT_UPDATE : FEATURE_KTTECH_NFC
// modify : kkong120215
// description : 
// S=====================================================================
// E=====================================================================

#define NFC_USE_MSM_XO
#define PN544_MAGIC 0xE9

#define PN544_SET_PWR _IOW(0xE9, 0x01, unsigned int)

// KTT_UPDATE : FEATURE_KTTECH_NFC
// modify : kkong120229, kkong120322
// description : 
// S=====================================================================
#define KTTECH_PN544_COMMAND _IOW(PN544_MAGIC, 0x05, unsigned int)
// E=====================================================================

struct pn544_i2c_platform_data {
  unsigned int irq_gpio;
  unsigned int ven_gpio;
  unsigned int firm_gpio;
  int (*setup_power) (struct device *dev);
  void (*shutdown_power) (struct device *dev);
  int (*setup_gpio) (int enable);
};

