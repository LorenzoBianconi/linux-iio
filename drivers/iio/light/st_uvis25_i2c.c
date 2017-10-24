/*
 * STMicroelectronics uvis25 i2c driver
 *
 * Copyright 2017 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/i2c.h>
#include <linux/slab.h>

#include "st_uvis25.h"

#define I2C_AUTO_INCREMENT	0x80

static int st_uvis25_i2c_read(struct device *dev, u8 addr, int len, u8 *data)
{
	if (len > 1)
		addr |= I2C_AUTO_INCREMENT;

	return i2c_smbus_read_i2c_block_data_or_emulated(to_i2c_client(dev),
							 addr, len, data);
}

static int st_uvis25_i2c_write(struct device *dev, u8 addr, int len, u8 *data)
{
	if (len > 1)
		addr |= I2C_AUTO_INCREMENT;

	return i2c_smbus_write_i2c_block_data(to_i2c_client(dev), addr,
					      len, data);
}

static const struct st_uvis25_transfer_function st_uvis25_transfer_fn = {
	.read = st_uvis25_i2c_read,
	.write = st_uvis25_i2c_write,
};

static int st_uvis25_i2c_probe(struct i2c_client *client,
			       const struct i2c_device_id *id)
{
	return st_uvis25_probe(&client->dev, client->irq,
			       &st_uvis25_transfer_fn);
}

static const struct of_device_id st_uvis25_i2c_of_match[] = {
	{ .compatible = "st,uvis25", },
	{},
};
MODULE_DEVICE_TABLE(of, st_uvis25_i2c_of_match);

static const struct i2c_device_id st_uvis25_i2c_id_table[] = {
	{ ST_UVIS25_DEV_NAME },
	{},
};
MODULE_DEVICE_TABLE(i2c, st_uvis25_i2c_id_table);

static struct i2c_driver st_uvis25_driver = {
	.driver = {
		.name = "st_uvis25_i2c",
		.pm = &st_uvis25_pm_ops,
		.of_match_table = of_match_ptr(st_uvis25_i2c_of_match),
	},
	.probe = st_uvis25_i2c_probe,
	.id_table = st_uvis25_i2c_id_table,
};
module_i2c_driver(st_uvis25_driver);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uvis25 i2c driver");
MODULE_LICENSE("GPL v2");
