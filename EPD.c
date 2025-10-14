#include "EPD.h"
#include <linux/delay.h>

const unsigned char EPD_2IN13_V2_lut_full_update[]= {
    0x80,0x60,0x40,0x00,0x00,0x00,0x00,             //LUT0: BB:     VS 0 ~7
    0x10,0x60,0x20,0x00,0x00,0x00,0x00,             //LUT1: BW:     VS 0 ~7
    0x80,0x60,0x40,0x00,0x00,0x00,0x00,             //LUT2: WB:     VS 0 ~7
    0x10,0x60,0x20,0x00,0x00,0x00,0x00,             //LUT3: WW:     VS 0 ~7
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT4: VCOM:   VS 0 ~7

    0x03,0x03,0x00,0x00,0x02,                       // TP0 A~D RP0
    0x09,0x09,0x00,0x00,0x02,                       // TP1 A~D RP1
    0x03,0x03,0x00,0x00,0x02,                       // TP2 A~D RP2
    0x00,0x00,0x00,0x00,0x00,                       // TP3 A~D RP3
    0x00,0x00,0x00,0x00,0x00,                       // TP4 A~D RP4
    0x00,0x00,0x00,0x00,0x00,                       // TP5 A~D RP5
    0x00,0x00,0x00,0x00,0x00,                       // TP6 A~D RP6

    0x15,0x41,0xA8,0x32,0x30,0x0A,
};

const unsigned char EPD_2IN13_V2_lut_partial_update[]= { //20 bytes
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT0: BB:     VS 0 ~7
    0x80,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT1: BW:     VS 0 ~7
    0x40,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT2: WB:     VS 0 ~7
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT3: WW:     VS 0 ~7
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,             //LUT4: VCOM:   VS 0 ~7

    0x0A,0x00,0x00,0x00,0x00,                       // TP0 A~D RP0
    0x00,0x00,0x00,0x00,0x00,                       // TP1 A~D RP1
    0x00,0x00,0x00,0x00,0x00,                       // TP2 A~D RP2
    0x00,0x00,0x00,0x00,0x00,                       // TP3 A~D RP3
    0x00,0x00,0x00,0x00,0x00,                       // TP4 A~D RP4
    0x00,0x00,0x00,0x00,0x00,                       // TP5 A~D RP5
    0x00,0x00,0x00,0x00,0x00,                       // TP6 A~D RP6

    0x15,0x41,0xA8,0x32,0x30,0x0A,
};

/*------------------------- 底层硬件操作 -------------------------*/
// 复位时序（保持原有时序参数）
static void EPD_Reset(void) {
    GPIO_Write(EPD_RST_PIN, 1);   // 注意：使用直接GPIO操作
    msleep(200);
    GPIO_Write(EPD_RST_PIN, 0);
    msleep(200);
    GPIO_Write(EPD_RST_PIN, 1);
    msleep(200);
}

// 发送命令（合并CS和DC控制）
static void EPD_SendCmd(UBYTE cmd) {
    GPIO_Write(EPD_DC_PIN, 0);    // DC=0表示命令
    GPIO_Write(EPD_CS_PIN, 0);
    SPI_TransferByte(cmd);      // 直接调用SPI传输
    GPIO_Write(EPD_CS_PIN, 1);
}

// 发送数据（优化为单次操作）
static void EPD_SendData(UBYTE dat) {
    GPIO_Write(EPD_DC_PIN, 1);    // DC=1表示数据
    GPIO_Write(EPD_CS_PIN, 0);
    SPI_TransferByte(dat);
    GPIO_Write(EPD_CS_PIN, 1);
}

// 忙等待
static void EPD_WaitBusy(void) {
    Debug("e-Paper busy\r\n");
    while(GPIO_Read(EPD_BUSY_PIN) == 1) {      //LOW: idle, HIGH: busy
        msleep(100);
    }
    Debug("e-Paper busy release\r\n");
}

/*------------------------- 显示控制 -------------------------*/
void EPD_RefreshDisplay(void) {
    EPD_SendCmd(0x22);
    EPD_SendData(0xC7);  // 0xC7:全刷, 0x0C:局刷
    EPD_SendCmd(0x20);
    EPD_WaitBusy();
}

void EPD_RefreshDisplayPart(void) {
    EPD_SendCmd(0x22);
    EPD_SendData(0x0C);  // 0xC7:全刷, 0x0C:局刷
    EPD_SendCmd(0x20);
    EPD_WaitBusy();
}

/*------------------------- 初始化 -------------------------*/
UBYTE DEV_Hardware_Init(void) {
    spi_init();

    // 初始化GPIO
    GPIO_Export();
    GPIO_Direction(EPD_BUSY_PIN, GPIO_IN);
    GPIO_Direction(EPD_RST_PIN,  GPIO_OUT);
    GPIO_Direction(EPD_DC_PIN,   GPIO_OUT);
    GPIO_Direction(EPD_CS_PIN,   GPIO_OUT);
    GPIO_Direction(EPD_PWR_PIN,  GPIO_OUT);
    
    // 设置默认电平
    GPIO_Write(EPD_CS_PIN, 1);
    GPIO_Write(EPD_PWR_PIN, 1);

    return 0;
}

void DEV_Hardware_Exit(void) {
    spi_close();

    // 重置所有GPIO状态
    GPIO_Write(EPD_CS_PIN, 0);
    GPIO_Write(EPD_PWR_PIN, 0);
    GPIO_Write(EPD_DC_PIN, 0);
    GPIO_Write(EPD_RST_PIN, 0);

    // 释放GPIO资源
    GPIO_Unexport(EPD_PWR_PIN);
    GPIO_Unexport(EPD_DC_PIN);
    GPIO_Unexport(EPD_RST_PIN);
    GPIO_Unexport(EPD_BUSY_PIN);
}

// 全刷参数
void EPD_init_full(void) {
    EPD_Reset();

    EPD_WaitBusy();
    EPD_SendCmd(0x12); // soft reset
    EPD_WaitBusy();

    EPD_SendCmd(0x74); //set analog block control
    EPD_SendData(0x54);
    EPD_SendCmd(0x7E); //set digital block control
    EPD_SendData(0x3B);

    EPD_SendCmd(0x01); //Driver output control
    EPD_SendData(0x27); // F9
    EPD_SendData(0x01); // 00
    EPD_SendData(0x01); // 00

    EPD_SendCmd(0x11); //data entry mode
    EPD_SendData(0x01);

    EPD_SendCmd(0x44); //set Ram-X address start/end position
    EPD_SendData(0x00);
    EPD_SendData(0x0F);    //0x0F-->(15+1)*8=128

    EPD_SendCmd(0x45); //set Ram-Y address start/end position
    EPD_SendData(0x27);   //0xF9-->(249+1)=250
    EPD_SendData(0x01);
    EPD_SendData(0x2E);
    EPD_SendData(0x00);

    EPD_SendCmd(0x3C); //BorderWavefrom
    EPD_SendData(0x03);

    EPD_SendCmd(0x2C); //VCOM Voltage
    EPD_SendData(0x55); //

    EPD_SendCmd(0x03);
    EPD_SendData(EPD_2IN13_V2_lut_full_update[70]);

    EPD_SendCmd(0x04); //
    EPD_SendData(EPD_2IN13_V2_lut_full_update[71]);
    EPD_SendData(EPD_2IN13_V2_lut_full_update[72]);
    EPD_SendData(EPD_2IN13_V2_lut_full_update[73]);

    EPD_SendCmd(0x3A);     //Dummy Line
    EPD_SendData(EPD_2IN13_V2_lut_full_update[74]);
    EPD_SendCmd(0x3B);     //Gate time
    EPD_SendData(EPD_2IN13_V2_lut_full_update[75]);

    EPD_SendCmd(0x32);
    
    UBYTE count;
    for(count = 0; count < 70; count++) {
        EPD_SendData(EPD_2IN13_V2_lut_full_update[count]);
    }

    EPD_SendCmd(0x4E);   // set RAM x address count to 0;
    EPD_SendData(0x00);
    EPD_SendCmd(0x4F);   // set RAM y address count to 0X127;
    EPD_SendData(0x27); // F9
    EPD_SendData(0x01); // 00
    EPD_WaitBusy();
}

/*------------------------- 高级功能 -------------------------*/
void EPD_Clear() {
    UWORD j,i;
    EPD_SendCmd(0x24);
    for (j = 0; j < HEIGHT; j++) {
        for (i = 0; i < WIDTH; i++) {
            EPD_SendData(0xFF);
        }
    }
    EPD_RefreshDisplay();
}

void EPD_Display(UBYTE *Image) {
    UWORD j,i;
    EPD_SendCmd(0x24);
    for (j = 0; j < HEIGHT; j++) {
        for (i = 0; i < WIDTH; i++) {
            EPD_SendData(Image[i + j * WIDTH]);
        }
    }
    EPD_RefreshDisplay();
}

void EPD_DisplayPart(UBYTE *Image)
{
    UWORD j,i;
    EPD_SendCmd(0x24);
    for (j = 0; j < HEIGHT; j++) {
        for (i = 0; i < WIDTH; i++) {
            EPD_SendData(Image[i + j * WIDTH]);
        }
    }

    EPD_RefreshDisplayPart();
}

void EPD_Sleep() {
    EPD_SendCmd(0x22);
    EPD_SendData(0xC3);
    EPD_SendCmd(0x20);
    
    EPD_SendCmd(0x10);
    EPD_SendData(0x01);
    msleep(100);
}
