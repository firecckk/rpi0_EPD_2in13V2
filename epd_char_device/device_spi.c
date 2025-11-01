#include "device_spi.h"

// SPI设备全局指针
struct spi_device *epd_spi_device = NULL;


// SPI驱动probe
static int epd_spi_probe(struct spi_device *spi)
{
    epd_spi_device = spi;
    spi->mode = SPI_MODE_0;
    spi->max_speed_hz = 10000000;
    spi_setup(spi);
    pr_info("epd_console: SPI device probed\n");
    return 0;
}

static void epd_spi_remove(struct spi_device *spi)
{
    epd_spi_device = NULL;
    pr_info("epd_console: SPI device removed\n");
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


int SPI_TransferByte(uint8_t data)
{
    uint8_t rx = 0;
    struct spi_transfer t = {
        .tx_buf = &data,
        .rx_buf = &rx,
        .len = 1,
    };
    struct spi_message m;
    spi_message_init(&m);
    spi_message_add_tail(&t, &m);
    if (!epd_spi_device) return 0xFF;
    spi_sync(epd_spi_device, &m);
    return rx;
}

int spi_init(void) {
    // 注册SPI设备
    struct spi_controller *master = NULL;
    //master = spi_controller_get(epd_spi_board_info.bus_num);
    //if (!master) {
    //    pr_err("epd_console: spi_busnum_to_master(%d) returned NULL\n", epd_spi_board_info.bus_num);
    //    return -ENODEV;
    //}
    epd_spi_device = spi_new_device(master, &epd_spi_board_info);
    if (!epd_spi_device) {
        pr_err("epd_console: Failed to create SPI device\n");
        return -ENODEV;
    }
    spi_register_driver(&epd_spi_driver);
    return 0;
}

void spi_close(void) {
    // 关闭硬件SPI
    spi_unregister_driver(&epd_spi_driver);
    if (epd_spi_device) spi_unregister_device(epd_spi_device);
}
