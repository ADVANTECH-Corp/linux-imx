# SPDX-License-Identifier: GPL-2.0

SHELL := /bin/bash
SDK_ENV := /opt/fsl-imx-xwayland/6.12-walnascar/environment-setup-armv8a-poky-linux
SDK_PATH := $(shell source "$(SDK_ENV)" && printf '%s' "$$PATH")
export PATH := $(SDK_PATH)
CC      := $(shell source "$(SDK_ENV)" && printf '%s' "$$CC")
CFLAGS  := $(shell source "$(SDK_ENV)" && printf '%s' "$$CFLAGS")
LDFLAGS := $(shell source "$(SDK_ENV)" && printf '%s' "$$LDFLAGS")


PROGS := slcanfd

all: $(PROGS)

clean:
	rm -fr $(PROGS)
