#!makefile

KERNEL_DIR := /usr/src/linux-headers-$(shell uname -r)

all:
	@$(MAKE) -C $(KERNEL_DIR) M=$$PWD modules

clean:
	@$(MAKE) -C $(KERNEL_DIR) M=$$PWD clean
