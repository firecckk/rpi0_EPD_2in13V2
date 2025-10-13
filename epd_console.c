#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/tty.h>
#include <linux/spi/spi.h>
#include <linux/gpio.h>
#include <linux/delay.h>

#include "EPD.h"

#define DRIVER_NAME "epd_tty"
#define EPD_TTY_MAJOR 240
#define EPD_TTY_MINORS 1

#define EPD_WIDTH 122
#define EPD_HEIGHT 250

// 全局变量
static struct tty_driver *epd_tty_driver;
static struct spi_device *epd_spi_device;

// SPI设备全局指针，供EPD.c等调用
struct spi_device *g_spi = NULL;

// 显示测试
static void test(UBYTE * image) {
    const int width = WIDTH;
    const int height = HEIGHT;
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width * 8; x++) {
            if ((x / 8 + y / 8) % 2 == 0) {       // 每 8x8 像素一个黑白块
                image[y * width + x / 8] |= (1 << (7 - x % 8));
            }
        }
    }
    
    EPD_Display(image);
    msleep(4000);

    EPD_Clear();
    msleep(500); 
}

// TTY操作函数集
static int epd_tty_open(struct tty_struct *tty, struct file *file)
{
    // 计算图像缓冲区大小
    const int buffer_size = WIDTH * HEIGHT;
 
    // 分配图像内存
    UBYTE *image = (UBYTE *)kmalloc(buffer_size, GFP_KERNEL);
    if(image == NULL) {
        Debug("Failed to allocate memory\n");
        return -1;
    }
    memset(image, 0x00, buffer_size);

    test(image);

    return 0;
}

static void epd_tty_close(struct tty_struct *tty, struct file *file)
{
    // 可添加清理代码
}

static ssize_t epd_tty_write(struct tty_struct *tty, const u8 *buf, size_t count)
{
    //epd_display_text(buf, count);
    return count;
}

static unsigned int epd_tty_write_room(struct tty_struct *tty)
{
    return 4096;
}

static const struct tty_operations epd_tty_ops = {
    .open = epd_tty_open,
    .close = epd_tty_close,
    .write = epd_tty_write,
    .write_room = epd_tty_write_room,
};

// 初始化TTY驱动
static int epd_tty_init(void)
{
    epd_tty_driver = tty_alloc_driver(EPD_TTY_MINORS, TTY_DRIVER_REAL_RAW);
    if (IS_ERR(epd_tty_driver))
        return PTR_ERR(epd_tty_driver);
    
    epd_tty_driver->driver_name = DRIVER_NAME;
    epd_tty_driver->name = "ttyEPD";
    epd_tty_driver->major = EPD_TTY_MAJOR;
    epd_tty_driver->minor_start = 0;
    epd_tty_driver->type = TTY_DRIVER_TYPE_CONSOLE;
    epd_tty_driver->subtype = SYSTEM_TYPE_CONSOLE;
    epd_tty_driver->init_termios = tty_std_termios;
    
    tty_set_operations(epd_tty_driver, &epd_tty_ops);
    
    int ret = tty_register_driver(epd_tty_driver);
    if (ret) {
        pr_err("Failed to register EPD TTY driver\n");
        tty_driver_kref_put(epd_tty_driver);
        return ret;
    }
    
    return 0;
}

// SPI驱动probe和remove
static int epd_spi_probe(struct spi_device *spi)
{
    g_spi = spi;
    spi->mode = SPI_MODE_0;
    spi->max_speed_hz = 10000000;
    spi_setup(spi);
    pr_info("epd_console: SPI device probed\n");
    return 0;
}

static int epd_spi_remove(struct spi_device *spi)
{
    g_spi = NULL;
    pr_info("epd_console: SPI device removed\n");
    return 0;
}

static struct spi_driver epd_spi_driver = {
    .driver = {
        .name = "epd_spi",
    },
    .probe = epd_spi_probe,
    .remove = epd_spi_remove,
};

// SPI板级信息（无需设备树）
static struct spi_board_info epd_spi_board_info = {
    .modalias = "epd_spi",
    .max_speed_hz = 10000000,
    .bus_num = 0,
    .chip_select = 0,
    .mode = SPI_MODE_0,
};

// 模块初始化和退出
static int __init epd_driver_init(void)
{
    pr_info("EPD Console Driver Initializing\n");

    // 注册SPI设备
    struct spi_master *master;
    master = spi_busnum_to_master(epd_spi_board_info.bus_num);
    if (!master) {
        pr_err("epd_console: spi_busnum_to_master(%d) returned NULL\n", epd_spi_board_info.bus_num);
        return -ENODEV;
    }
    epd_spi_device = spi_new_device(master, &epd_spi_board_info);
    if (!epd_spi_device) {
        pr_err("epd_console: Failed to create SPI device\n");
        return -ENODEV;
    }
    spi_register_driver(&epd_spi_driver);

    // 初始化GPIO SPI
    DEV_Hardware_Init();
    // 初始化屏幕
    EPD_init_full();

    EPD_Clear();
    msleep(500);
    
    // 初始化tty驱动
    int ret = epd_tty_init();
    if (ret)
        return ret;
        
    return 0;
}

static void __exit epd_driver_exit(void)
{
    tty_unregister_driver(epd_tty_driver);
    tty_driver_kref_put(epd_tty_driver);

    spi_unregister_driver(&epd_spi_driver);
    if (epd_spi_device)
        spi_unregister_device(epd_spi_device);

    DEV_Hardware_Exit();
    
    pr_info("EPD Console Driver Removed\n");
}

module_init(epd_driver_init);
module_exit(epd_driver_exit);

MODULE_AUTHOR("author");
MODULE_DESCRIPTION("E-Paper Display Console Driver");
MODULE_LICENSE("GPL");
