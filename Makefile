# 目标模块与依赖文件
obj-m := epd_console.o
epd_console-objs := epd_console.o EPD.o RPI_gpio.o dev_hardware_SPI.o

# 内核路径与编译规则
KDIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

#ccflags-y := -std=gnu99

# 交叉编译配置（可选）
# ARCH ?= arm
# CROSS_COMPILE ?= arm-linux-gnueabihf-
# export ARCH CROSS_COMPILE

all:
	$(MAKE) -C $(KDIR) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
