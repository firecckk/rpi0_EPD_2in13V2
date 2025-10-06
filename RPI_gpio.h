/* RPI_gpio.h - Linux内核GPIO驱动接口 */
#ifndef _RPI_GPIO_H_
#define _RPI_GPIO_H_

#include <linux/gpio.h>
#include <linux/printk.h>

/* 方向定义 */
#define GPIO_IN  1
#define GPIO_OUT 0

/* 调试宏 */
#define GPIO_Debug(fmt, ...) pr_info("GPIO: " fmt, ##__VA_ARGS__)

/* 函数声明 */
int GPIO_Export(void);
int GPIO_Direction(int Pin, int Dir);
int GPIO_Read(int Pin);
int GPIO_Write(int Pin, int value);
int GPIO_Unexport(int Pin);

#endif /* _RPI_GPIO_H_ */
