# Copyright 2006 The Android Open Source Project

LOCAL_PATH:= $(call my-dir)
include $(CLEAR_VARS)

include $(CLEAR_VARS)
LOCAL_SRC_FILES := main.c asr.c mdm.c tty2tcp.c sahara.c tftp.c ymodem.c unisoc.c usb_linux.c ftp.c mtk.c sony.c
LOCAL_CFLAGS += -pie -fPIE
LOCAL_CFLAGS += -DUSE_NDK
LOCAL_LDFLAGS += -pie -fPIE
LOCAL_MODULE_TAGS:= optional
LOCAL_MODULE:= QAndroidLog
include $(BUILD_EXECUTABLE)
