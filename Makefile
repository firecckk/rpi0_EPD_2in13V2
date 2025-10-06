# 内核模块编译Makefile
obj-m := epd_console.o
epd_console-objs := obj/epd_console.o obj/EPD.o obj/RPI_gpio.o obj/dev_Hardware_SPI.o

# 内核源码目录（根据你的树莓派内核版本修改）
KDIR := /lib/modules/$(shell uname -r)/build
# 当前目录
PWD := $(shell pwd)

EXTRA_CFLAGS += -I$(KDIR)/include

DEBUG ?= 1
ifeq ($(DEBUG),1)
    EXTRA_CFLAGS += -DDEBUG -g
endif

# 创建obj目录
OBJ_DIR := obj
$(shell mkdir -p $(OBJ_DIR))

# 默认编译目标
all: $(epd_console-objs)
	$(MAKE) -C $(KDIR) M=$(PWD) modules

# 自定义规则：将.c文件编译到obj目录
obj/%.o: %.c
	$(CC) $(CFLAGS) $(EXTRA_CFLAGS) -c $< -o $@

# 清理构建文件
clean:
	$(MAKE) -C $(KDIR) M=$(PWD) clean
	rm -rf $(OBJ_DIR)

.PHONY: all clean
