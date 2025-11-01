#ifndef DEVICE_SPI_H
#define DEVICE_SPI_H

#include <linux/spi/spi.h>

int spi_init(void);
void spi_close(void);
int SPI_TransferByte(uint8_t data);

#endif
