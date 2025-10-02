#include <stdio.h>
#include <stdlib.h>

#include "lib/font/font12.c"
#include "EPD.h"

void DEV_Delay_ms(UDOUBLE xms) {
    for(UDOUBLE i=0; i<xms; i++) usleep(1000);
}

void GenerateTestPattern(UBYTE *image) {
    const int width = WIDTH;
    const int height = HEIGHT;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width * 8; x++) {
            if ((x / 8 + y / 8) % 2 == 0) {       // 每 8x8 像素一个黑白块
                image[y * width + x / 8] |= (1 << (7 - x % 8));
            }
        }
    }
}

void drawFront(UBYTE *image, UWORD x, UWORD y, const char *text, bool color) {
    for(int i = y; i<y+12; i++) {
        image[i] = Font12_Table[12*1+i];
    }
}

UBYTE DEV_Module_Init(void) {
    // 初始化GPIO
    GPIOD_Export();
    GPIOD_Direction(EPD_BUSY_PIN, GPIOD_IN);
    GPIOD_Direction(EPD_RST_PIN,  GPIOD_OUT);
    GPIOD_Direction(EPD_DC_PIN,   GPIOD_OUT);
    GPIOD_Direction(EPD_CS_PIN,   GPIOD_OUT);
    GPIOD_Direction(EPD_PWR_PIN,  GPIOD_OUT);
    
    // 设置默认电平
    GPIOD_Write(EPD_CS_PIN, 1);
    GPIOD_Write(EPD_PWR_PIN, 1);

    // 初始化硬件SPI
    DEV_HARDWARE_SPI_begin("/dev/spidev0.0");
    DEV_HARDWARE_SPI_setSpeed(10000000);
    return 0;
}

void DEV_Module_Exit(void) {
    // 关闭硬件SPI
    DEV_HARDWARE_SPI_end();

    // 重置所有GPIO状态
    GPIOD_Write(EPD_CS_PIN, 0);
    GPIOD_Write(EPD_PWR_PIN, 0);
    GPIOD_Write(EPD_DC_PIN, 0);
    GPIOD_Write(EPD_RST_PIN, 0);

    // 释放GPIO资源
    GPIOD_Unexport(EPD_PWR_PIN);
    GPIOD_Unexport(EPD_DC_PIN);
    GPIOD_Unexport(EPD_RST_PIN);
    GPIOD_Unexport(EPD_BUSY_PIN);
    GPIOD_Unexport_GPIO();
}

int main() {
    printf("EPD_2IN13_V2 Demo Start\n");
    
    DEV_Module_Init();
    
    EPD_init_full();
    EPD_Clear();
    DEV_Delay_ms(500);

    // 计算图像缓冲区大小
    const int buffer_size = WIDTH * HEIGHT;
 
    // 分配图像内存
    UBYTE *image = (UBYTE *)malloc(buffer_size);
    if(image == NULL) {
        printf("Failed to allocate memory\n");
        return -1;
    }

    // 演示1: 全刷模式
    printf("Full refresh test\n"); 
    EPD_RefreshDisplay();
    
    // 生成测试图案并显示
    GenerateTestPattern(image);
    EPD_Display(image);
    printf("image displayed\n");
    DEV_Delay_ms(10000);
    
    // 清屏
    EPD_Clear();
    DEV_Delay_ms(1000);

    // 进入睡眠模式
    EPD_Sleep();
    
    // 释放资源
    free(image);
    DEV_Module_Exit();
    
    printf("Demo finished\n");
    return 0;
}
