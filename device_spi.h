#ifndef DEVICE_SPI_H
#define DEVICE_SPI_H

#include <linux/spi/spi.h>

// SPI设备全局指针
struct spi_device *g_spi = NULL;

int spi_init();
int spi_close();
int SPI_TransferByte(uint8_t data)

#endif