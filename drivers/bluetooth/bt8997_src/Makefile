#
# Makefile for UART Linux Bluetooth HCI device drivers.
#


ifneq ($(M),)
CONFIG_FILE := $(M)/.config
-include $(CONFIG_FILE)
else
CONFIG_FILE := $(PWD)/.config
-include $(CONFIG_FILE)
endif

ifeq ($(CONFIG_BG2), y)
CONFIG_PXA9XX=y
CONFIG_PXA3XX=y
KERNELDIR ?= /usr/src/arm/linux-3.0.8-bg2
CROSS_COMPILE ?= /usr/local/arm-eabi-4.4.3/bin/arm-eabi-
endif

ifeq ($(CONFIG_HIKEY), y)
ARCH ?= arm64
CROSS_COMPILE ?= /usr/local/arm/aarch64-linux-android-4.9/bin/aarch64-linux-android-
KERNELDIR ?= /usr/src/arm/Kernel_hikey960_AndroidP_kernel4.9
endif

ifeq ($(CONFIG_MMP2), y)
ARCH ?= arm
KERNELDIR ?= /home/marvell/mmp2/android/kernel/kernel
CROSS_COMPILE ?= /home/marvell/mmp2/android/vendor/marvell/generic/toolchain/arm-marvell-linux-gnueabi-vfp-4.2.0/bin/arm-marvell-linux-gnueabi-
endif

ifeq ($(CONFIG_T3T), y)
ARCH ?= arm
ifeq ($(KVER),3)
ifeq ($(KMINORVER),110_JB)
KERNELDIR ?= /usr/src/arm/linux-3.1.10-JB-T3T/
else
KERNELDIR ?= /usr/src/arm/linux-3.1.10-T3T/
endif
else
ifeq ($(KMINORVER),39)
KERNELDIR ?= /usr/src/arm/linux-2.6.39-T3T
else
KERNELDIR ?= /Projects2/Tegra/T3T/kernel/linux-2.6/out
endif
endif
CROSS_COMPILE ?= /usr/local/arm-eabi-4.4.3/bin/arm-eabi-
endif

ifeq ($(CONFIG_PXA978T), y)
CONFIG_PXA9XX=y
endif
ifeq ($(CONFIG_PXA955), y)
CONFIG_PXA9XX=y
endif
ifeq ($(CONFIG_PXA950), y)
CONFIG_PXA9XX=y
endif
ifeq ($(CONFIG_PXA920), y)
CONFIG_PXA9XX=y
endif
ifeq ($(CONFIG_PXA9XX), y)
EXTRA_CFLAGS += -DPXA9XX
ifeq ($(CONFIG_PXA955), y)
EXTRA_CFLAGS += -DPXA955
KERNELDIR ?= /home/marvell/pxa_955/pxa955_froyo_147/kernel/kernel
CROSS_COMPILE ?= /home/marvell/pxa_955/pxa955_froyo_147/prebuilt/linux-x86/toolchain/arm-eabi-4.4.0/bin/arm-eabi-
else
ifeq ($(CONFIG_PXA950), y)
EXTRA_CFLAGS += -DPXA950
KERNELDIR ?= /usr/src/arm/linux-2.6.29-pxa950
CROSS_COMPILE ?= /usr/local/arm-marvell-linux-gnueabi/bin/arm-marvell-linux-gnueabi-
else
ifeq ($(CONFIG_PXA978T), y)
KERNELDIR ?= /usr/src/arm/linux-3.0.8-pxa978t
CROSS_COMPILE ?= /usr/local/arm-marvell-linux-gnueabi/bin/arm-marvell-linux-gnueabi-
else
ifeq ($(CONFIG_PXA920), y)
EXTRA_CFLAGS += -DPXA920
ifeq ($(KVER),3)
KERNELDIR ?= /usr/src/arm/linux-3.0.8-pxa920
else
ifeq ($(KMINORVER),35)
KERNELDIR ?= /usr/src/arm/linux-2.6.35-pxa920
else
ifeq ($(KMINORVER),32)
KERNELDIR ?= /usr/src/arm/linux-2.6.32-pxa920
else
KERNELDIR ?= /usr/src/arm/linux-2.6.29-pxa920
endif
endif
endif
CROSS_COMPILE ?= /usr/local/arm-marvell-linux-gnueabi/bin/arm-marvell-linux-gnueabi-
else
ifeq ($(KMINORVER),25)
KERNELDIR ?= /usr/src/arm/linux-2.6.25-pxa9xx
CROSS_COMPILE ?= /usr/local/arm/4.1.1/bin/arm-linux-
else
KERNELDIR ?= /usr/src/arm/linux-2.6.24-pxa9xx
CROSS_COMPILE ?= /usr/local/arm/4.1.1/bin/arm-linux-
endif
endif
endif
endif
endif
ARCH ?= arm
else
ifeq ($(CONFIG_MOORESTOWN), y)
KERNELDIR ?= /home/marvell/release_20100316/kernel
CROSS_COMPILE ?= /home/marvell/release_20100316/prebuilt/linux-x86/toolchain/i686-unknown-linux-gnu-4.2.1/bin/i686-unknown-linux-gnu-
else
KERNELDIR ?= /lib/modules/$(shell uname -r)/build
endif
endif

# Debug Option
# DEBUG LEVEL n/1/2:
# n: NO DEBUG
# 1: PRINTM(MSG,...), PRINTM(FATAL,...), PRINTM(WARN,...) and PRINTM(INFO,...)
# 2: All PRINTM()
CONFIG_DEBUG=1

ifeq ($(CONFIG_DEBUG),1)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
endif

ifeq ($(CONFIG_DEBUG),2)
	EXTRA_CFLAGS += -DDEBUG_LEVEL1
	EXTRA_CFLAGS += -DDEBUG_LEVEL2
endif

	EXTRA_CFLAGS += -DFPNUM='"XX"'

ifeq ($(CONFIG_AMP), y)
EXTRA_CFLAGS += -DBT_AMP
ifneq ($(CONFIG_MBT_EXT),y)
	EXTRA_CFLAGS += -I$(PWD)/include
endif
endif

ifeq ($(CONFIG_MBT_EXT),y)
	EXTRA_CFLAGS += -DMBT_EXT
endif

# Enable BT_HCIUART_PS and MULTI_CARD_PS_SUPPORT
CONFIG_BT_HCIUART_PS=y
CONFIG_MULTI_CARD_PS_SUPPORT=y

ifeq ($(CONFIG_BT_HCIUART_PS), y)
EXTRA_CFLAGS += -DCONFIG_BT_HCIUART_PS
ifeq ($(CONFIG_MULTI_CARD_PS_SUPPORT), y)
EXTRA_CFLAGS += -DCONFIG_MULTI_CARD_PS
endif
endif

hci_uart-y							:= mbt_char.o
hci_uart-y							+= hci_ldisc.o
hci_uart-y							+= hci_wrapper.o
hci_uart-$(CONFIG_BT_HCIUART_H4)	+= hci_h4.o
hci_uart-$(CONFIG_BT_HCIUART_BCSP)	+= hci_bcsp.o
hci_uart-$(CONFIG_BT_HCIUART_LL)	+= hci_ll.o
hci_uart-$(CONFIG_BT_HCIUART_PS)	+= hci_ps.o
hci_uart_8997-objs				:= $(hci_uart-y)

obj-m	:= hci_uart_8997.o

BINDIR = ../bin_muart
KMODPATH := kernel/drivers/bluetooth

PWD   := $(shell pwd)

default:
ifeq ($(CONFIG_PXA9XX), y)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
else
ifeq ($(CONFIG_HIKEY), y)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
else
ifeq ($(CONFIG_MMP2), y)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
else
ifeq ($(CONFIG_T3T), y)
	$(MAKE) -C $(KERNELDIR) M=$(PWD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
else
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
endif
endif
endif
endif

build: default
	@if [ ! -d $(BINDIR) ]; then \
		mkdir $(BINDIR); \
	fi
	@cp -f hci_uart.ko $(BINDIR)
	@cp -f README $(BINDIR)

clean:
	find . -name "*.o" -exec rm {} \;
	find . -name "*.*~" -exec rm {} \;
	find . -name "*.d" -exec rm {} \;
	find . -name "*.mod.c" -exec rm {} \;
	find . -name "*.ko" -exec rm {} \;
	find . -name "*.symvers" -exec rm {} \;
	find . -name ".*.cmd" -exec rm {} \;
	rm -rf .tmp_versions

install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install INSTALL_MOD_DIR=$(KMODPATH)

PXA9XX PXA955 PXA950 PXA920 X86 MOORESTOWN BG2 PXA978T HIKEY MMP2 T3T:
	@echo CONFIG_$@=y > $(CONFIG_FILE)

3.1.10_JB:
	@echo "KVER=3" >> $(CONFIG_FILE)
	@echo "KMINORVER=110_JB" >> $(CONFIG_FILE)

3.1.10:
	@echo "KVER=3" >> $(CONFIG_FILE)
	@echo "KMINORVER=110" >> $(CONFIG_FILE)

3.0.8:
	@echo "KVER=3" >> $(CONFIG_FILE)
	@echo "KMINORVER=08" >> $(CONFIG_FILE)

2.6.25:
	@echo "KMINORVER=25" >> $(CONFIG_FILE)

2.6.32:
	@echo "KMINORVER=32" >> $(CONFIG_FILE)

2.6.35:
	@echo "KMINORVER=35" >> $(CONFIG_FILE)

2.6.39:
	@echo "KMINORVER=39" >> $(CONFIG_FILE)

AMP:
	@echo "CONFIG_AMP=y" >> $(CONFIG_FILE)

MBT_EXT:
	@echo "CONFIG_MBT_EXT=y" >> $(CONFIG_FILE)
