#ifndef EPD_H
#define EPD_H

#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include "Debug.h"
#include "RPI_gpiod.h"
#include "dev_hardware_SPI.h"

#define EPD_2IN13_V2_WIDTH      122 
#define EPD_2IN13_V2_HEIGHT     250 

#define WIDTH ((EPD_2IN13_V2_WIDTH % 8 == 0)? (EPD_2IN13_V2_WIDTH / 8 ): (EPD_2IN13_V2_WIDTH / 8 + 1))
#define HEIGHT (EPD_2IN13_V2_HEIGHT) 

// 数据类型定义
#define UBYTE   uint8_t
#define UWORD   uint16_t
#define UDOUBLE uint32_t

// 硬件引脚定义（直接初始化）
static const int EPD_RST_PIN     = 17;
static const int EPD_DC_PIN      = 25;
static const int EPD_CS_PIN      = 8;
static const int EPD_BUSY_PIN    = 24;
static const int EPD_PWR_PIN     = 18;
static const int EPD_MOSI_PIN    = 10;
static const int EPD_SCLK_PIN    = 11;

static void EPD_Reset();
static void EPD_SendCmd(UBYTE Reg);
static void EPD_SendData(UBYTE Data);
static void EPD_WaitBusy(void);
void EPD_RefreshDisplay(void);
void EPD_RefreshDisplayPart(void);

void EPD_init_full(void);
void EPD_Clear(void);
void EPD_Display(UBYTE *Image);
void EPD_DisplayPart(UBYTE *Image);
void EPD_Sleep(void);

#endif
