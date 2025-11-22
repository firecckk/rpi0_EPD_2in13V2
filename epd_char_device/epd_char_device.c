#include <linux/module.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/delay.h>
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/spi/spi.h>
#include <linux/gpio/consumer.h>
#include <linux/cdev.h>

#include "../lib/font/font12.c"

#include "epd_2in13v2.c"

#define MAX_CHAR_COUNT 256

/* Char device */
struct char_mem_dev {
    char * text;
    size_t size;
    size_t cap;
    struct mutex lock;
}

static struct char_mem_dev char_dev;

static int epd_open(struct inode *inode, struct file *filp) {
    pr_info("opening epd device\n");
    struct epd_dev *epd = container_of(inode->i_cdev, struct epd_dev, cdev);
    filp->private_data = epd;
    
    return 0;
}

static ssize_t epd_read(struct file *filp, char __user *buf, 
                          size_t count, loff_t *f_pos)
{
    pr_info("epd_read: count %d, f_pos %d\n", count, *f_pos);
    ssize_t retval = 0;
    size_t bytes_to_read = count;

    if (mutex_lock_interruptible(&char_dev.lock))
        return -ERESTARTSYS;

    if(*f_pos >= char_dev.size) {
        retval = 0;
        goto out;
    }

    if(*f_pos + count > char_dev.size)
        bytes_to_read = char_dev.size - *f_pos;

    if(copy_to_user(buf, char_dev.data + *f_pos, bytes_to_read)) {
        retval = -EFAULT;
        goto out;
    }

    pr_info("char_mem: Read %zu bytes from position %lld\n", 
        bytes_to_read, *f_pos);
        
    *f_pos += bytes_to_read;
    retval = bytes_to_read;

    out:
    mutex_unlock(&char_dev.lock);
    return retval;
}

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
    
    EPD_print(epd, count)
    
    kfree(text_buf);
    return count;
}

static int epd_release(struct inode *inode, struct file *filp) {
    pr_info("closing epd char device\n");
    return 0;
}

// static loff_t epd_llseek(struct file *filp, loff_t offset, int whence)
// {
//     loff_t new_pos;
//     struct epd_dev *epd = filp->private_data;
    
//     if (mutex_lock_interruptible(epd->lock))
//         return -ERESTARTSYS;
    
//     switch (whence) {
//     case SEEK_SET:  // 从文件开始处定位
//         new_pos = offset;
//         break;
//     case SEEK_CUR:  // 从当前位置定位
//         new_pos = filp->f_pos + offset;
//         break;
//     case SEEK_END:  // 从文件末尾定位
//         new_pos = char_dev.size + offset;
//         break;
//     default:
//         mutex_unlock(epd->lock);
//         return -EINVAL;
//     }
    
//     // 检查位置是否有效
//     if (new_pos < 0) {
//         mutex_unlock(epd->lock);
//         return -EINVAL;
//     }
    
//     // 允许定位到文件末尾之后（但不能超过容量）
//     if (new_pos > char_dev.capacity) {
//         mutex_unlock(epd->lock);
//         return -ENOSPC;
//     }
    
//     filp->f_pos = new_pos;
//     mutex_unlock(epd->lock);
    
//     pr_info("char_mem: Seek to position %lld\n", new_pos);
//     return new_pos;
// }

static const struct file_operations epd_fops = {
    .owner = THIS_MODULE,
    .open = epd_open,
    .read = epd_read,
    .write = epd_write,
    .release = epd_release,
    //.llseek = epd_llseek,
};

/* Module Load / Unload*/
static struct class *epd_class;

static int epd_probe(struct spi_device *spi)
{
    pr_info("epd probe");
    struct device *dev = &spi->dev;
    struct epd_dev *epd;
    int ret;

    // init spi device
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

    // init EPD
    epd->spi->mode = SPI_MODE_0;
    epd->spi->bits_per_word = 8;
    epd->spi->max_speed_hz = 1000000;
    spi_setup(epd->spi);
    
    EPD_Init(epd);

    // init text buffer
    char_dev.size = 0;
    char_dev.cap = MAX_CHAR_COUNT;
    char_dev.data = kzalloc(MAX_CHAR_COUNT, GFP_KERNEL);

    // register char device
    ret = alloc_chrdev_region(&epd->devt, 0, 1, "epd");
    if (ret < 0)
        return ret;

    cdev_init(&epd->cdev, &epd_fops);
    epd->cdev.owner = THIS_MODULE;
    ret = cdev_add(&epd->cdev, epd->devt, 1);
    if (ret < 0) {
        pr_err("Failed to register char device\n");
        unregister_chrdev_region(epd->devt, 1);
        kfree(char_dev.data);
        return ret;
    }
    device_create(epd_class, dev, epd->devt, epd, "epd%d", 0);
    
    return 0;
}

/* 清理（devm_资源会自动释放），注销字符设备等 */
static void epd_remove(struct spi_device *spi)
{
    pr_info("epd remove");
    struct epd_dev *epd = spi_get_drvdata(spi);
    EPD_Clear(epd);
    kfree(epd->display_buf);
    
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
    .probe = epd_probe,
    .remove = epd_remove,
};


static int __init my_init(void)
{
    int ret;

    pr_info("init epd module");
    epd_class = class_create("epd");
    if (IS_ERR(epd_class))
        return PTR_ERR(epd_class);

    ret = spi_register_driver(&epd_spi_driver);
    if (ret < 0) {
	pr_info("can't register epd spi device. exiting ...");
        class_destroy(epd_class);
        return ret;
    }

    return 0;
}

static void __exit my_exit(void)
{
    spi_unregister_driver(&epd_spi_driver);
    class_destroy(epd_class);
    if (char_dev.data)
        kfree(char_dev.data);
}

module_init(my_init);
module_exit(my_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ck");
MODULE_DESCRIPTION("Waveshare 2.13inch V2 EPD char device driver");
