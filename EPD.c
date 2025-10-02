#include "EPD.h"

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
static void EPD_Reset() {
    GPIOD_Write(EPD_RST_PIN, 1);   // 注意：使用直接GPIO操作
    usleep(200000);
    GPIOD_Write(EPD_RST_PIN, 0);
    usleep(2000);
    GPIOD_Write(EPD_RST_PIN, 1);
    usleep(200000);
}

// 发送命令（合并CS和DC控制）
static void EPD_SendCmd(uint8_t cmd) {
    GPIOD_Write(EPD_DC_PIN, 0);    // DC=0表示命令
    GPIOD_Write(EPD_CS_PIN, 0);
    DEV_HARDWARE_SPI_TransferByte(cmd);      // 直接调用SPI传输
    GPIOD_Write(EPD_CS_PIN, 1);
}

// 发送数据（优化为单次操作）
static void EPD_SendData(uint8_t dat) {
    GPIOD_Write(EPD_DC_PIN, 1);    // DC=1表示数据
    GPIOD_Write(EPD_CS_PIN, 0);
    DEV_HARDWARE_SPI_TransferByte(dat);
    GPIOD_Write(EPD_CS_PIN, 1);
}

// 忙等待
static void EPD_WaitBusy(void) {
    Debug("e-Paper busy\r\n");
    while(GPIOD_Read(EPD_BUSY_PIN) == 1) {      //LOW: idle, HIGH: busy
        usleep(100000);
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
    EPD_SendData(0xF9);
    EPD_SendData(0x00);
    EPD_SendData(0x00);

    EPD_SendCmd(0x11); //data entry mode
    EPD_SendData(0x01);

    EPD_SendCmd(0x44); //set Ram-X address start/end position
    EPD_SendData(0x00);
    EPD_SendData(0x0F);    //0x0F-->(15+1)*8=128

    EPD_SendCmd(0x45); //set Ram-Y address start/end position
    EPD_SendData(0xF9);   //0xF9-->(249+1)=250
    EPD_SendData(0x00);
    EPD_SendData(0x00);
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
    for(UBYTE count = 0; count < 70; count++) {
        EPD_SendData(EPD_2IN13_V2_lut_full_update[count]);
    }

    EPD_SendCmd(0x4E);   // set RAM x address count to 0;
    EPD_SendData(0x00);
    EPD_SendCmd(0x4F);   // set RAM y address count to 0X127;
    EPD_SendData(0xF9);
    EPD_SendData(0x00);
    EPD_WaitBusy();
}

/*------------------------- 高级功能 -------------------------*/
void EPD_Clear() {
    EPD_SendCmd(0x24);
    for (UWORD j = 0; j < HEIGHT; j++) {
        for (UWORD i = 0; i < WIDTH; i++) {
            EPD_SendData(0xFF);
        }
    }
    EPD_RefreshDisplay();
}

void EPD_Display(uint8_t *Image) {
    EPD_SendCmd(0x24);
    for (UWORD j = 0; j < HEIGHT; j++) {
        for (UWORD i = 0; i < WIDTH; i++) {
            EPD_SendData(Image[i + j * WIDTH]);
        }
    }
    EPD_RefreshDisplay();
}

void EPD_DisplayPart(UBYTE *Image)
{
    EPD_SendCmd(0x24);
    for (UWORD j = 0; j < HEIGHT; j++) {
        for (UWORD i = 0; i < WIDTH; i++) {
            EPD_SendData(Image[i + j * WIDTH]);
        }
    }

    EPD_RefreshDisplay();
}

void EPD_Sleep() {
    EPD_SendCmd(0x22);
    EPD_SendData(0xC3);
    EPD_SendCmd(0x20);
    
    EPD_SendCmd(0x10);
    EPD_SendData(0x01);
    usleep(100000);
}
