#include <linux/gpio.h>
#include <linux/printk.h>
#include <linux/string.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

#include "RPI_gpio.h"

#define NUM_MAXBUF 128
#define GPIO_IN  1
#define GPIO_OUT 0

// 调试宏（替换原GPIOD_Debug）
#define GPIO_Debug(fmt, ...) pr_info("GPIO: " fmt, ##__VA_ARGS__)

// GPIO芯片号（动态检测）
static int gpio_chip_num = -1;

// 导出GPIO
int GPIO_Export(void)
{
    struct file *fp;
    char buffer[NUM_MAXBUF];
    
    // 模拟popen（内核不能直接执行shell命令）
    fp = filp_open("/proc/cpuinfo", O_RDONLY, 0);
    if (IS_ERR(fp)) {
        GPIO_Debug("Cannot open /proc/cpuinfo\n");
        return -ENOENT;
    }
    
    kernel_read(fp, buffer, sizeof(buffer), 0);
    filp_close(fp, NULL);
    
    if (strstr(buffer, "Raspberry Pi 5")) {
        gpio_chip_num = 4; // Pi 5使用gpiochip4
    } else {
        gpio_chip_num = 0; // 其他型号默认gpiochip0
    }
    
    return 0;
}

// 设置GPIO方向
int GPIO_Direction(int Pin, int Dir)
{
    int ret;
    
    if (!gpio_is_valid(Pin)) {
        GPIO_Debug("Invalid GPIO: %d\n", Pin);
        return -EINVAL;
    }
    
    ret = gpio_request(Pin, "epd_gpio");
    if (ret) {
        GPIO_Debug("Request failed for GPIO %d\n", Pin);
        return ret;
    }
    
    if (Dir == GPIO_IN) {
        ret = gpio_direction_input(Pin);
        GPIO_Debug("Pin%d: input\n", Pin);
    } else {
        ret = gpio_direction_output(Pin, 0);
        GPIO_Debug("Pin%d: output\n", Pin);
    }
    
    if (ret) {
        gpio_free(Pin);
        return ret;
    }
    
    return 0;
}

// 读取GPIO值
int GPIO_Read(int Pin)
{
    if (!gpio_is_valid(Pin)) {
        GPIO_Debug("Invalid GPIO: %d\n", Pin);
        return -EINVAL;
    }
    
    return gpio_get_value(Pin);
}

// 写入GPIO值
int GPIO_Write(int Pin, int value)
{
    if (!gpio_is_valid(Pin)) {
        GPIO_Debug("Invalid GPIO: %d\n", Pin);
        return -EINVAL;
    }
    
    gpio_set_value(Pin, value);
    return 0;
}

// 释放GPIO
int GPIO_Unexport(int Pin)
{
    if (gpio_is_valid(Pin)) {
        gpio_free(Pin);
        GPIO_Debug("Unexport: Pin%d\n", Pin);
    }
    return 0;
}
