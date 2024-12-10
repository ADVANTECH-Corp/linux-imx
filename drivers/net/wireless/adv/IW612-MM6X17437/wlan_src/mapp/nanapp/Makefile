# File : nanapp/Makefile
#
# Copyright 2023 NXP

# Path to the top directory of the wlan distribution
PATH_TO_TOP = ../..

ifeq ($(findstring IMX_ANDROID,$(ccflags-y)),IMX_ANDROID)
CC=/usr/local/arm/android-ndk-r23-beta5/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android21-clang
endif

# Determine how we should copy things to the install directory
ABSPATH := $(filter /%, $(INSTALLDIR))
RELPATH := $(filter-out /%, $(INSTALLDIR))
INSTALLPATH := $(ABSPATH)
ifeq ($(strip $(INSTALLPATH)),)
INSTALLPATH := $(PATH_TO_TOP)/$(RELPATH)
endif

# Override CFLAGS for application sources, remove __ kernel namespace defines
CFLAGS := $(filter-out -D__%, $(ccflags-y))
# remove KERNEL include dir
CFLAGS := $(filter-out -I$(KERNELDIR)%, $(CFLAGS))


#CFLAGS += -DAP22 -fshort-enums -DMWU_IPC_UDP
CFLAGS += -g -Wall -DOS_LINUX  -DMWU_SERVER -DSTDOUT_DEBUG
CFLAGS += -Wno-stringop-truncation -Wno-packed-bitfield-compat
#ECHO = @
ifeq (,$(findstring ANDROID_KERNEL, $(CFLAGS)))
LIBS=-lrt
endif



# This should somehow come from main Makefile
CONFIG_NAN = y
CONFIG_OPENSSL = y

ifeq ($(CONFIG_NAN), y)
CFLAGS += -DCONFIG_NAN
CFLAGS += -DCONFIG_LOCATION
NAN_SECURITY = y
CONFIG_NAN2 = y
CONFIG_NAN3 = y
endif

ifeq ($(CONFIG_NAN2), y)
CFLAGS += -DCONFIG_NAN2 -DDOT11AZ
endif

ifeq ($(CONFIG_NAN3), y)
CFLAGS += -DCONFIG_NAN3
endif

.PHONY: default tags all

VPATH += nan os/linux wsc wsc/encrypt_src mlocationmod

ifeq ($(CONFIG_NAN), y)
OBJECTS = mwu_main.o mwu_cli.o mwu_client.o mwu_if_manager.o mwu.o mwu_log.o mwu_timer.o wps_os.o wps_l2.o
OBJECTS += mlocation.o mlocation_mwu.o mlocation_lib.o wps_wlan.o range_kalman.o
OBJECTS +=  nan_mwu.o nan.o nan_lib.o data_engine.o discovery_engine.o bloom_filter_compute_crc.o sha256.o mwu_ioctl.o
ifeq ($(NAN_SECURITY), y)
OBJECTS += nan_security.o nan_eapol.o
CFLAGS += -DNAN_SECURITY
CFLAGS += -DOPENSSL
CFLAGS += -I/opt/openssl-3.1.1-imx/include/openssl/
CFLAGS += -I/opt/openssl-3.1.1-imx/include/
LIBS += -L/opt/openssl-3.1.1-imx/
LIBS += -lssl -lcrypto
endif
ifeq ($(CONFIG_NAN2), y)
OBJS += data_engine.o
endif
CFLAGS += -I. -Ios/include -Inan -Iwsc/encrypt_src -Imlocationmod
ifeq ($(CONFIG_OPENSSL), y)
OBJECTS += sha1.o md5.o
endif
endif

HEADERS = *.h

TARGET = nanapp

build appsbuild default: $(TARGET)
	@cp -f $(TARGET) $(INSTALLPATH)

all : tags default

$(TARGET): $(OBJECTS) $(HEADERS)
	$(ECHO)$(CC) -o $@ $(OBJECTS) -lm $(LIBS)

%.o: %.c $(HEADERS)
	$(ECHO)$(CC) $(CFLAGS) -c -o $@ $<

tags:
	ctags -R -f tags.txt

distclean clean:
	$(ECHO)$(RM) $(OBJECTS) $(TARGET) *.o
	$(ECHO)$(RM) tags.txt

