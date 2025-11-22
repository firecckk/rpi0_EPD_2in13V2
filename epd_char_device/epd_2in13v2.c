
/**
* EPD driver
**/
#define EPD_2IN13_V2_WIDTH      122 
#define EPD_2IN13_V2_HEIGHT     250 

#define WIDTH ((EPD_2IN13_V2_WIDTH % 8 == 0)? (EPD_2IN13_V2_WIDTH / 8 ): (EPD_2IN13_V2_WIDTH / 8 + 1))
#define HEIGHT (EPD_2IN13_V2_HEIGHT) 

struct epd_dev {
    struct spi_device *spi;
    struct gpio_desc *gdc;
    struct gpio_desc *grst;
    struct gpio_desc *gbusy;
    struct gpio_desc *gpwr;
    struct cdev cdev;
    dev_t devt;
    uint8_t * display_buf;
};

const uint8_t EPD_2IN13_V2_lut_full_update[]= {
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

static void EPD_Reset(struct epd_dev *epd) {
    pr_info("== EPD Hardware Reset ==");
    gpiod_set_value(epd->grst, 1);
    msleep(200);
    gpiod_set_value(epd->grst, 0);
    msleep(200);
    gpiod_set_value(epd->grst, 1);
    msleep(200);
}

static uint8_t EPD_TransferByte(struct epd_dev *epd, uint8_t data) 
{
    uint8_t rx = 0;
    struct spi_transfer xfer = {
        .tx_buf = &data,
        .rx_buf = &rx,
        .len = 1,
        .speed_hz = 500000,  // 20MHz
        .bits_per_word = 8,
	.cs_change = 0,  // CS keep
    };
    struct spi_message msg;

    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    spi_sync(epd->spi, &msg);
    
    return rx;
}

static void EPD_SendCmd(struct epd_dev *epd, uint8_t cmd) {
    gpiod_set_value(epd->gdc, 0); // cmd
    EPD_TransferByte(epd, cmd);
}

static void EPD_SendData(struct epd_dev *epd, uint8_t dat) {
    gpiod_set_value(epd->gdc, 1); // data
    EPD_TransferByte(epd, dat);
}

static void EPD_WaitBusy(struct epd_dev *epd) {
    uint8_t timeout = 50;
    while(gpiod_get_value(epd->gbusy) == 1) {      //LOW: idle, HIGH: busy
	timeout--;
        msleep(100);
        //pr_info("e-Paper busy\r\n");
	if(timeout <= 0) {
		pr_debug("EPD wait busy time out. Force release in software\r\n");
		break;
	}
    }
    pr_info("e-Paper busy release\r\n");
}

static void EPD_RefreshDisplay(struct epd_dev *epd) {
    EPD_SendCmd(epd, 0x22);
    EPD_SendData(epd, 0xC7);  // 0xC7:全刷, 0x0C:局刷
    EPD_SendCmd(epd, 0x20);
    EPD_WaitBusy(epd);
}

static void EPD_Clear(struct epd_dev *epd) {
    uint8_t j,i;
    EPD_SendCmd(epd, 0x24);
    for (j = 0; j < HEIGHT; j++) {
        for (i = 0; i < WIDTH; i++) {
            EPD_SendData(epd, 0xFF);
        }
    }
    EPD_RefreshDisplay(epd);
}

// 全刷参数
static void EPD_init_full(struct epd_dev *epd) {
    EPD_Reset(epd);

    EPD_WaitBusy(epd);
    EPD_SendCmd(epd, 0x12); // soft reset
    EPD_WaitBusy(epd);

    EPD_SendCmd(epd, 0x74); //set analog block control
    EPD_SendData(epd, 0x54);
    EPD_SendCmd(epd, 0x7E); //set digital block control
    EPD_SendData(epd, 0x3B);

    EPD_SendCmd(epd, 0x01); //Driver output control
    EPD_SendData(epd, 0x27); // F9
    EPD_SendData(epd, 0x01); // 00
    EPD_SendData(epd, 0x01); // 00

    EPD_SendCmd(epd, 0x11); //data entry mode
    EPD_SendData(epd, 0x01);

    EPD_SendCmd(epd, 0x44); //set Ram-X address start/end position
    EPD_SendData(epd, 0x00);
    EPD_SendData(epd, 0x0F);    //0x0F-->(15+1)*8=128

    EPD_SendCmd(epd, 0x45); //set Ram-Y address start/end position
    EPD_SendData(epd, 0x27);   //0xF9-->(249+1)=250
    EPD_SendData(epd, 0x01);
    EPD_SendData(epd, 0x2E);
    EPD_SendData(epd, 0x00);

    EPD_SendCmd(epd, 0x3C); //BorderWavefrom
    EPD_SendData(epd, 0x03);

    EPD_SendCmd(epd, 0x2C); //VCOM Voltage
    EPD_SendData(epd, 0x55); //

    EPD_SendCmd(epd, 0x03);
    EPD_SendData(epd, EPD_2IN13_V2_lut_full_update[70]);

    EPD_SendCmd(epd, 0x04); //
    EPD_SendData(epd, EPD_2IN13_V2_lut_full_update[71]);
    EPD_SendData(epd, EPD_2IN13_V2_lut_full_update[72]);
    EPD_SendData(epd, EPD_2IN13_V2_lut_full_update[73]);

    EPD_SendCmd(epd, 0x3A);     //Dummy Line
    EPD_SendData(epd, EPD_2IN13_V2_lut_full_update[74]);
    EPD_SendCmd(epd, 0x3B);     //Gate time
    EPD_SendData(epd, EPD_2IN13_V2_lut_full_update[75]);

    EPD_SendCmd(epd, 0x32);
    
    uint8_t count;
    for(count = 0; count < 70; count++) {
        EPD_SendData(epd, EPD_2IN13_V2_lut_full_update[count]);
    }

    EPD_SendCmd(epd, 0x4E);   // set RAM x address count to 0;
    EPD_SendData(epd, 0x00);
    EPD_SendCmd(epd, 0x4F);   // set RAM y address count to 0X127;
    EPD_SendData(epd, 0x27); // F9
    EPD_SendData(epd, 0x01); // 00
    EPD_WaitBusy(epd);
}

static int EPD_Init(struct epd_dev *epd)
{
    pr_info("Init epd device");

    // create display buffer
    epd->display_buf = kmalloc(WIDTH * HEIGHT, GFP_KERNEL);
    if (!epd->display_buf) {
        return -ENOMEM;
    }
    memset(epd->display_buf, 0, WIDTH * HEIGHT);

    EPD_init_full(epd);
    EPD_Clear(epd);
    return 0;
}

static void EPD_Flush(struct epd_dev *epd)
{
    EPD_SendCmd(epd, 0x24);  // WRITE_RAM
    for(int i = 0; i < WIDTH * HEIGHT; i++) {
        EPD_SendData(epd, epd->display_buf[i]);
    }
}

#define LANDSCAPE
static void EPD_DrawChar(struct epd_dev *epd, uint16_t x, uint16_t y, char ch) {
    uint8_t width = Font12.Width;
    uint8_t height = Font12.Height;
    const uint8_t *ptr = &Font12.table[(ch - ' ') * height];

    for(uint8_t j = 0; j < height; j++) {
        uint8_t line = ptr[j];
        for(uint8_t i = 0; i < width; i++) {
            if(line & 0x80) {
#ifndef LANDSCAPE
                uint16_t real_x = x + i;
                uint16_t real_y = y + j;
#else
                uint16_t real_x = EPD_2IN13_V2_WIDTH - y - j;
                uint16_t real_y = x + i;
#endif
                if(real_x < EPD_2IN13_V2_WIDTH && real_y < EPD_2IN13_V2_HEIGHT) {
                    uint16_t byte_pos = real_y * WIDTH + real_x / 8;
                    uint8_t bit_pos = 7 - (real_x % 8);
                    epd->display_buf[byte_pos] |= (1 << bit_pos);
                }
            }
            line <<= 1;
        }
    }
}

#ifndef LANDSCAPE        
#define BUF_WIDTH EPD_2IN13_V2_WIDTH
#define BUF_HEIGHT EPD_2IN13_V2_HEIGHT
#else
#define BUF_WIDTH EPD_2IN13_V2_HEIGHT
#define BUF_HEIGHT EPD_2IN13_V2_WIDTH
#endif	

static void EPD_print(struct epd_dev *epd, char *text_buf, size_t count) {
    EPD_Clear(epd);

    int i;
    uint16_t x = 0, y = 0;
    
    // 渲染文本
    for(i = 0; i < count; i++) {
        if(text_buf[i] == '\n') {
            x = 0;
            y += Font12.Height;
            continue;
        }

        if(x + Font12.Width > BUF_WIDTH) {
            x = 0;
            y += Font12.Height;
        }
        
        if(y + Font12.Height > BUF_HEIGHT) {
	    pr_info("epd chars out of bound %d - portrait", EPD_2IN13_V2_WIDTH);
            break;  // 超出显示范围
        }
        EPD_DrawChar(epd, x, y, text_buf[i]);
        x += Font12.Width;
    }
    
    EPD_Flush(epd);
    EPD_RefreshDisplay(epd);
}
