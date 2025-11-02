#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/cdev.h>
#include <linux/bitrev.h>

#include "../lib/font/font12.c"

static struct class *epd_class;

struct epd_dev {
    struct spi_device *spi;
    struct gpio_desc *gdc;
    struct gpio_desc *grst;
    struct gpio_desc *gbusy;
    struct gpio_desc *gpwr;
    struct cdev cdev;
    dev_t devt;
    /* ...其他状态... */
};

/**
* EPD driver
**/
#define EPD_2IN13_V2_WIDTH      122 
#define EPD_2IN13_V2_HEIGHT     250 

#define WIDTH ((EPD_2IN13_V2_WIDTH % 8 == 0)? (EPD_2IN13_V2_WIDTH / 8 ): (EPD_2IN13_V2_WIDTH / 8 + 1))
#define HEIGHT (EPD_2IN13_V2_HEIGHT) 

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
    //EPD_TransferByte(epd, bitrev8(cmd));
    EPD_TransferByte(epd, cmd);
}

static void EPD_SendData(struct epd_dev *epd, uint8_t dat) {
    gpiod_set_value(epd->gdc, 1); // data
    //EPD_TransferByte(epd, bitrev8(dat));
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

static void EPD_Init(struct epd_dev *epd)
{
    pr_info("Init epd device");

    // Init EPD
    EPD_init_full(epd);
}

/* Kernel API */
static int epd_open(struct inode *inode, struct file *filp) {
    pr_info("opening epd device\n");
    struct epd_dev *epd = container_of(inode->i_cdev, struct epd_dev, cdev);
    filp->private_data = epd;
    
    // Init EPD
    EPD_Init(epd);

    EPD_Clear(epd);
    return 0;
}

static void EPD_DrawChar(struct epd_dev *epd, uint16_t x, uint16_t y, char ch) {
    uint8_t width = Font12.Width;
    uint8_t height = Font12.Height;
    const uint8_t *ptr = &Font12.table[(ch - ' ') * height];
    uint8_t *display_buf = kmalloc(WIDTH * HEIGHT, GFP_KERNEL);
    if (!display_buf) {
        pr_err("Failed to allocate display buffer\n");
        return;
    }
    memset(display_buf, 0, WIDTH * HEIGHT);
    
    // 首先将字符数据收集到显示缓冲区
    for(uint8_t j = 0; j < height; j++) {
        uint8_t line = ptr[j];
        for(uint8_t i = 0; i < width; i++) {
            if(line & 0x80) {
                uint16_t real_x = x + i;
                uint16_t real_y = y + j;
                if(real_x < EPD_2IN13_V2_WIDTH && real_y < EPD_2IN13_V2_HEIGHT) {
                    uint16_t byte_pos = real_y * WIDTH + real_x / 8;
                    uint8_t bit_pos = 7 - (real_x % 8);
                    display_buf[byte_pos] |= (1 << bit_pos);
                }
            }
            line <<= 1;
        }
    }
    
    // 一次性发送整个显示缓冲区
    EPD_SendCmd(epd, 0x24);  // WRITE_RAM
    for(int i = 0; i < WIDTH * HEIGHT; i++) {
        EPD_SendData(epd, display_buf[i]);
    }
    kfree(display_buf);
}

#define MAX_CHAR_COUNT 256  // 最大字符数

static ssize_t epd_write(struct file *filp, const char __user *buf,
                        size_t count, loff_t *f_pos) {
    struct epd_dev *epd = filp->private_data;
    char *text_buf;
    int i;
    uint16_t x = 0, y = 0;
    
    if(count > MAX_CHAR_COUNT) {
        return -EINVAL;
    }
    
    // 分配临时缓冲区
    text_buf = kmalloc(count + 1, GFP_KERNEL);
    if (!text_buf)
        return -ENOMEM;
    
    // 从用户空间复制文本数据
    if (copy_from_user(text_buf, buf, count)) {
        kfree(text_buf);
        return -EFAULT;
    }
    text_buf[count] = '\0';
    
    // 清屏
    EPD_Clear(epd);
    
    // 渲染文本
    for(i = 0; i < count; i++) {
        if(text_buf[i] == '\n') {
            x = 0;
            y += Font12.Height;
            continue;
        }
        
        if(x + Font12.Width > EPD_2IN13_V2_WIDTH) {
            x = 0;
            y += Font12.Height;
        }
        
        if(y + Font12.Height > EPD_2IN13_V2_HEIGHT) {
            break;  // 超出显示范围
        }
        
        EPD_DrawChar(epd, x, y, text_buf[i]);
        x += Font12.Width;
    }
    
    // 刷新显示
    EPD_RefreshDisplay(epd);
    
    kfree(text_buf);
    return count;
}

static int epd_release(struct inode *inode, struct file *filp) {
    pr_info("closing epd char device\n");
    return 0;
}

static const struct file_operations epd_fops = {
    .owner = THIS_MODULE,
    .write = epd_write,
    .open = epd_open,
    .release = epd_release,
};

static int epd_spi_probe(struct spi_device *spi)
{
    pr_info("epd probe");
    struct device *dev = &spi->dev;
    struct epd_dev *epd;
    int ret;

    epd = devm_kzalloc(dev, sizeof(*epd), GFP_KERNEL);
    if (!epd)
        return -ENOMEM;

    epd->spi = spi;
    spi_set_drvdata(spi, epd);

    epd->gdc = devm_gpiod_get(dev, "dc", GPIOD_OUT_LOW);
    epd->grst = devm_gpiod_get(dev, "reset", GPIOD_OUT_HIGH);
    epd->gbusy = devm_gpiod_get(dev, "busy", GPIOD_IN);
    epd->gpwr = devm_gpiod_get(dev, "pwr", GPIOD_OUT_HIGH);

    if (IS_ERR(epd->gdc) || IS_ERR(epd->grst) || 
        IS_ERR(epd->gbusy) || IS_ERR(epd->gpwr)) {
        dev_err(dev, "failed to get gpios\n");
        return -ENODEV;
    }

    ret = alloc_chrdev_region(&epd->devt, 0, 1, "epd");
    if (ret < 0)
        return ret;

    cdev_init(&epd->cdev, &epd_fops);
    epd->cdev.owner = THIS_MODULE;
    ret = cdev_add(&epd->cdev, epd->devt, 1);
    if (ret < 0)
        goto err_unreg_chrdev;

    device_create(epd_class, dev, epd->devt, epd, "epd%d", 0);

    // 初始化 SPI 设备
    epd->spi->mode = SPI_MODE_0;
    epd->spi->bits_per_word = 8;
    epd->spi->max_speed_hz = 500000;
    spi_setup(epd->spi);
    
    return 0;

err_unreg_chrdev:
    unregister_chrdev_region(epd->devt, 1);
    return ret;
}

/* 清理（devm_资源会自动释放），注销字符设备等 */
static void epd_spi_remove(struct spi_device *spi)
{
    pr_info("epd remove");
    struct epd_dev *epd = spi_get_drvdata(spi);
    EPD_Clear(epd);
    
    device_destroy(epd_class, epd->devt);
    cdev_del(&epd->cdev);
    unregister_chrdev_region(epd->devt, 1);
}

/* of_match_table */
static const struct of_device_id epd_of_match[] = {
    { .compatible = "waveshare,epd2in13v2" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, epd_of_match);

/* 注册 spi_driver，使用 of_match_table */
static struct spi_driver epd_spi_driver = {
    .driver = {
        .name = "epd2in13v2",
        .of_match_table = epd_of_match,
    },
    .probe = epd_spi_probe,
    .remove = epd_spi_remove,
};

/* 修改模块 init/exit：注册 spi_driver */
static int __init my_init(void)
{
    int ret;

    epd_class = class_create("epd");  // Remove THIS_MODULE parameter
    if (IS_ERR(epd_class))
        return PTR_ERR(epd_class);

    ret = spi_register_driver(&epd_spi_driver);
    if (ret < 0) {
        class_destroy(epd_class);
        return ret;
    }

    return 0;
}

static void __exit my_exit(void)
{
    spi_unregister_driver(&epd_spi_driver);
    class_destroy(epd_class);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ck");
MODULE_DESCRIPTION("Waveshare 2.13inch V2 EPD char device driver");
