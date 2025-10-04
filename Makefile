# 编译器设置
CC := gcc
CFLAGS := -Wall -Wextra -MMD -MP
LDFLAGS := -lgpiod

# 调试模式选项（默认开启 DEBUG 宏）
DEBUG ?= 1
ifeq ($(DEBUG), 1)
    CFLAGS += -g -O0 -DDEBUG
else
    CFLAGS += -O2
endif

# 目录设置
SRC_DIR := .
OBJ_DIR := obj
BIN_DIR := bin

# 源文件列表
SRCS := clear.c epd_test.c EPD.c dev_hardware_SPI.c RPI_gpiod.c
OBJS := $(SRCS:%.c=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)  # 依赖文件（.d）

# 可执行文件
EXECUTABLES := clear_screen epd_test
BIN_TARGETS := $(EXECUTABLES:%=$(BIN_DIR)/%)

# 默认构建所有可执行文件
all: $(BIN_TARGETS)

# 创建目录（如果不存在）
$(OBJ_DIR):
	mkdir -p $(OBJ_DIR)

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

# 编译规则：.c -> .o
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# 链接规则：.o -> 可执行文件
$(BIN_DIR)/clear_screen: $(OBJ_DIR)/clear.o $(OBJ_DIR)/EPD.o $(OBJ_DIR)/dev_hardware_SPI.o $(OBJ_DIR)/RPI_gpiod.o | $(BIN_DIR)
	$(CC) $^ $(LDFLAGS) -o $@

$(BIN_DIR)/epd_test: $(OBJ_DIR)/epd_test.o $(OBJ_DIR)/EPD.o $(OBJ_DIR)/dev_hardware_SPI.o $(OBJ_DIR)/RPI_gpiod.o | $(BIN_DIR)
	$(CC) $^ $(LDFLAGS) -o $@

# 包含自动生成的依赖文件
-include $(DEPS)

# 清理构建文件
clean:
	rm -rf $(OBJ_DIR) $(BIN_DIR)

.PHONY: all clean
