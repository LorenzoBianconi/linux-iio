/*
 * STMicroelectronics sensors spi library driver
 *
 * Copyright 2012-2013 STMicroelectronics Inc.
 *
 * Denis Ciocca <denis.ciocca@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_SENSORS_SPI_H
#define ST_SENSORS_SPI_H

#include <linux/spi/spi.h>
#include <linux/iio/common/st_sensors.h>

#ifdef CONFIG_OF
void st_sensors_of_spi_probe(struct spi_device *spi,
			     const struct of_device_id *match);
#else
static inline void st_sensors_of_spi_probe(struct spi_device *spi,
					   const struct of_device_id *match)
{
}
#endif

void st_sensors_spi_configure(struct iio_dev *indio_dev,
			struct spi_device *spi, struct st_sensor_data *sdata);

#endif /* ST_SENSORS_SPI_H */
