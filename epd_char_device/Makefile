obj-m := epd_char_device.o

LINUX_SRC := ~/linux
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

# 添加编译标志
ccflags-y := -std=gnu99 -Wall

all:
    $(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
    $(MAKE) -C $(KDIR) M=$(PWD) clean
    rm -f *.o *.ko *.mod.c *.mod.o *.order *.symvers

# 检查代码风格
checkpatch:
    $(LINUX_SRC)/scripts/checkpatch.pl -f epd_char_device.c