#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/cdev.h>

struct epd_dev {
    struct spi_device *spi;
    struct gpio_desc *gdc;
    struct gpio_desc *grst;
    struct gpio_desc *gbusy;
    struct gpio_desc *gcs;
    struct gpio_desc *gpwr;
    struct cdev cdev;
    dev_t devt;
    /* ...其他状态... */
};

static void EPD_Reset(struct epd_dev *epd) {
    gpiod_set_value(epd->grst, 1);
    msleep(200);
    gpiod_set_value(epd->grst, 0);
    msleep(200);
    gpiod_set_value(epd->grst, 1);
    msleep(200);
}

static void EPD_SendCmd(struct epd_dev *epd, unsigned char cmd) {
    gpiod_set_value(epd->gdc, 0); // cmd
    gpiod_set_value(epd->gcs, 0);
    spi_write(epd->spi, &cmd, 1);
    gpiod_set_value(epd->gcs, 1);
}

static void EPD_SendData(struct epd_dev *epd, unsigned char dat) {
    gpiod_set_value(epd->gdc, 1); // data
    gpiod_set_value(epd->gcs, 0);
    spi_write(epd->spi, &dat, 1);
    gpiod_set_value(epd->gcs, 1);
}

static void EPD_WaitBusy(struct epd_dev *epd) {
    pr_info("e-Paper busy\r\n");
    while(gpiod_get_value(epd->gbusy) == 1) {      //LOW: idle, HIGH: busy
        msleep(100);
    }
    pr_info("e-Paper busy release\r\n");
}

static int epd_open(struct inode *inode, struct file *filp) {
    struct epd_dev *epd = container_of(inode->i_cdev, struct epd_dev, cdev);
    filp->private_data = epd;
    EPD_Reset(epd);
    return 0;
}

static ssize_t epd_write(struct file *filp, const char __user *buf,
                        size_t count, loff_t *f_pos) {
    struct epd_dev *epd = filp->private_data;
    pr_info("writing to epd device\n");
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

static uint8_t EPD_TransferByte(struct epd_dev *epd, uint8_t data) 
{
    uint8_t rx = 0;
    struct spi_transfer xfer = {
        .tx_buf = &data,
        .rx_buf = &rx,
        .len = 1,
        .delay_usecs = 5,      // 对应原代码中的 DEV_HARDWARE_SPI_SetDataInterval(5)
        .speed_hz = 20000000,  // 对应原代码中的 setSpeed(20000000)
        .bits_per_word = 8,
    };
    struct spi_message msg;

    spi_message_init(&msg);
    spi_message_add_tail(&xfer, &msg);
    spi_sync(epd->spi, &msg);
    
    return rx;
}

static void EPD_Init(struct epd_dev *epd)
{
    // 设置 SPI 模式
    epd->spi->mode = SPI_MODE_0;
    epd->spi->bits_per_word = 8;
    epd->spi->max_speed_hz = 20000000;
    spi_setup(epd->spi);

    // 初始化 GPIO
    gpiod_set_value(epd->gcs, 1);  // CS 初始高电平
    EPD_Reset(epd);                 // 执行复位序列
}

static int epd_spi_probe(struct spi_device *spi)
{
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
    epd->gcs = devm_gpiod_get(dev, "cs", GPIOD_OUT_HIGH);
    epd->gpwr = devm_gpiod_get(dev, "pwr", GPIOD_OUT_HIGH);

    if (IS_ERR(epd->gdc) || IS_ERR(epd->grst) || 
        IS_ERR(epd->gbusy) || IS_ERR(epd->gcs) || IS_ERR(epd->gpwr)) {
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
    EPD_Init(epd);

    return 0;

err_unreg_chrdev:
    unregister_chrdev_region(epd->devt, 1);
    return ret;
}

/* 清理（devm_资源会自动释放），注销字符设备等 */
static int epd_spi_remove(struct spi_device *spi)
{
    struct epd_dev *epd = spi_get_drvdata(spi);
    
    device_destroy(epd_class, epd->devt);
    cdev_del(&epd->cdev);
    unregister_chrdev_region(epd->devt, 1);
    
    return 0;
}

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

    epd_class = class_create(THIS_MODULE, "epd");
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

/* of_match_table */
static const struct of_device_id epd_of_match[] = {
    { .compatible = "waveshare,epd2in13v2" },
    { /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, epd_of_match);