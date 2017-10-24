/*
 * STMicroelectronics uvis25 spi driver
 *
 * Copyright 2017 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/spi/spi.h>
#include <linux/slab.h>

#include "st_uvis25.h"

#define SENSORS_SPI_READ	0x80
#define SPI_AUTO_INCREMENT	0x40

static int st_uvis25_spi_read(struct device *dev, u8 addr, int len, u8 *data)
{
	struct spi_device *spi = to_spi_device(dev);
	struct iio_dev *iio_dev = spi_get_drvdata(spi);
	struct st_uvis25_hw *hw = iio_priv(iio_dev);
	int err;

	struct spi_transfer xfers[] = {
		{
			.tx_buf = hw->tb.tx_buf,
			.bits_per_word = 8,
			.len = 1,
		},
		{
			.rx_buf = hw->tb.rx_buf,
			.bits_per_word = 8,
			.len = len,
		}
	};

	if (len > 1)
		addr |= SPI_AUTO_INCREMENT;
	hw->tb.tx_buf[0] = addr | SENSORS_SPI_READ;

	err = spi_sync_transfer(spi, xfers,  ARRAY_SIZE(xfers));
	if (err < 0)
		return err;

	memcpy(data, hw->tb.rx_buf, len * sizeof(u8));

	return len;
}

static int st_uvis25_spi_write(struct device *dev, u8 addr, int len, u8 *data)
{
	struct iio_dev *iio_dev;
	struct st_uvis25_hw *hw;
	struct spi_device *spi;

	if (len >= ST_UVIS25_TX_MAX_LENGTH)
		return -ENOMEM;

	spi = to_spi_device(dev);
	iio_dev = spi_get_drvdata(spi);
	hw = iio_priv(iio_dev);

	hw->tb.tx_buf[0] = addr;
	memcpy(&hw->tb.tx_buf[1], data, len);

	return spi_write(spi, hw->tb.tx_buf, len + 1);
}

static const struct st_uvis25_transfer_function st_uvis25_transfer_fn = {
	.read = st_uvis25_spi_read,
	.write = st_uvis25_spi_write,
};

static int st_uvis25_spi_probe(struct spi_device *spi)
{
	return st_uvis25_probe(&spi->dev, spi->irq,
			       &st_uvis25_transfer_fn);
}

static const struct of_device_id st_uvis25_spi_of_match[] = {
	{ .compatible = "st,uvis25", },
	{},
};
MODULE_DEVICE_TABLE(of, st_uvis25_spi_of_match);

static const struct spi_device_id st_uvis25_spi_id_table[] = {
	{ ST_UVIS25_DEV_NAME },
	{},
};
MODULE_DEVICE_TABLE(spi, st_uvis25_spi_id_table);

static struct spi_driver st_uvis25_driver = {
	.driver = {
		.name = "st_uvis25_spi",
		.pm = &st_uvis25_pm_ops,
		.of_match_table = of_match_ptr(st_uvis25_spi_of_match),
	},
	.probe = st_uvis25_spi_probe,
	.id_table = st_uvis25_spi_id_table,
};
module_spi_driver(st_uvis25_driver);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uvis25 spi driver");
MODULE_LICENSE("GPL v2");
