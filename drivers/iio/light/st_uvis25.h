/*
 * STMicroelectronics uvis25 sensor driver
 *
 * Copyright 2017 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#ifndef ST_UVIS25_H
#define ST_UVIS25_H

#define ST_UVIS25_DEV_NAME		"uvis25"

#include <linux/iio/iio.h>

#define ST_UVIS25_RX_MAX_LENGTH		8
#define ST_UVIS25_TX_MAX_LENGTH		8

struct st_uvis25_transfer_buffer {
	u8 rx_buf[ST_UVIS25_RX_MAX_LENGTH];
	u8 tx_buf[ST_UVIS25_TX_MAX_LENGTH] ____cacheline_aligned;
};

struct st_uvis25_transfer_function {
	int (*read)(struct device *dev, u8 addr, int len, u8 *data);
	int (*write)(struct device *dev, u8 addr, int len, u8 *data);
};

/**
 * struct st_uvis25_hw - ST UVIS25 sensor instance
 * @dev: Pointer to instance of struct device (I2C or SPI).
 * @lock: Mutex to protect read and write operations.
 * @trig: The trigger in use by the driver.
 * @enabled: Status of the sensor (false->off, true->on).
 * @irq: Device interrupt line (I2C or SPI).
 * @tf: Transfer function structure used by I/O operations.
 * @tb: Transfer buffers used by SPI I/O operations.
 */
struct st_uvis25_hw {
	struct device *dev;

	struct mutex lock;
	struct iio_trigger *trig;
	bool enabled;
	int irq;

	const struct st_uvis25_transfer_function *tf;
	struct st_uvis25_transfer_buffer tb;
};

extern const struct dev_pm_ops st_uvis25_pm_ops;

int st_uvis25_write_with_mask(struct st_uvis25_hw *hw, u8 addr,
			      u8 mask, u8 val);
int st_uvis25_set_enable(struct st_uvis25_hw *hw, bool enable);
int st_uvis25_probe(struct device *dev, int irq,
		    const struct st_uvis25_transfer_function *tf_ops);
int st_uvis25_allocate_buffer(struct iio_dev *iio_dev);
int st_uvis25_allocate_trigger(struct iio_dev *iio_dev);

#endif /* ST_UVIS25_H */
