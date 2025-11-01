obj-m := epd_chardev.o
epd-objs := epd_char_device.o device_spi.o

LINUX_SRC := ~/linux
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

ccflags-y := -std=gnu99

all:
        $(MAKE) -C $(KDIR) M=$(PWD) modules

mknode:
        mknod /dev/hello c 236 0

checkpatch:
        $(LINUX_SRC)/scripts/checkpatch.pl -f char_device.c

clean:
        $(MAKE) -C $(KDIR) M=$(PWD) clean