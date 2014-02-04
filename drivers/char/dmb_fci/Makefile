#DMB device driver makefile
EXTRA_CFLAGS += -Wno-error
obj-$(CONFIG_KTTECH_TDMB_SERVICE) += dmb.o
dmb-objs := dmbdrv_kttech.o bbm.o fc8050_bb.o fc8050_isr.o fc8050_spi.o fc8050_i2c.o fc8050_ppi.o fc8050_hpi.o fc8050_tun.o fci_hal.o fci_i2c.o fci_oal.o fci_ringbuffer.o fci_tun.o fic.o ficdecoder.o
