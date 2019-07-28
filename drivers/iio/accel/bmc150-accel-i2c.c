// SPDX-License-Identifier: GPL-2.0-only
/*
 * 3-axis accelerometer driver supporting following I2C Bosch-Sensortec chips:
 *  - BMC150
 *  - BMI055
 *  - BMA255
 *  - BMA250E
 *  - BMA222E
 *  - BMA280
 *
 * Copyright (c) 2014, Intel Corporation.
 */

#include <linux/device.h>
#include <linux/mod_devicetable.h>
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/acpi.h>
#include <linux/regmap.h>

#include "bmc150-accel.h"

static int bmc150_accel_probe(struct i2c_client *client,
			      const struct i2c_device_id *id)
{
	int ret;
	struct acpi_device *adev;
	struct i2c_board_info board_info;
	struct i2c_client *second_dev;
	struct regmap *regmap;
	const char *name = NULL;
	bool block_supported =
		i2c_check_functionality(client->adapter, I2C_FUNC_I2C) ||
		i2c_check_functionality(client->adapter,
					I2C_FUNC_SMBUS_READ_I2C_BLOCK);

	regmap = devm_regmap_init_i2c(client, &bmc150_regmap_conf);
	if (IS_ERR(regmap)) {
		dev_err(&client->dev, "Failed to initialize i2c regmap\n");
		return PTR_ERR(regmap);
	}

	if (id)
		name = id->name;

	ret = bmc150_accel_core_probe(&client->dev, regmap, client->irq, name,
				       block_supported);
	if (ret)
		return ret;

	/*
	 * Some BOSC0200 acpi_devices describe 2 accelerometers in a single ACPI
	 * device, try instantiating a second i2c_client for an I2cSerialBusV2
	 * ACPI resource with index 1.
	 */
	adev = ACPI_COMPANION(&client->dev);
	if (adev && strcmp(acpi_device_hid(adev), "BOSC0200") == 0) {
		memset(&board_info, 0, sizeof(board_info));
		strlcpy(board_info.type, "bma250e", I2C_NAME_SIZE);
		second_dev = i2c_acpi_new_device(&client->dev, 1, &board_info);
		if (second_dev)
			bmc150_set_second_device(second_dev);
	}

	return ret;
}

static int bmc150_accel_remove(struct i2c_client *client)
{
	struct i2c_client *second_dev = bmc150_get_second_device(client);

	if (second_dev)
		i2c_unregister_device(second_dev);

	return bmc150_accel_core_remove(&client->dev);
}

static const struct acpi_device_id bmc150_accel_acpi_match[] = {
	{"BSBA0150",	bmc150},
	{"BMC150A",	bmc150},
	{"BMI055A",	bmi055},
	{"BMA0255",	bma255},
	{"BMA250E",	bma250e},
	{"BMA222E",	bma222e},
	{"BMA0280",	bma280},
	{"BOSC0200"},
	{ },
};
MODULE_DEVICE_TABLE(acpi, bmc150_accel_acpi_match);

static const struct i2c_device_id bmc150_accel_id[] = {
	{"bmc150_accel",	bmc150},
	{"bmi055_accel",	bmi055},
	{"bma255",		bma255},
	{"bma250e",		bma250e},
	{"bma222e",		bma222e},
	{"bma280",		bma280},
	{}
};

MODULE_DEVICE_TABLE(i2c, bmc150_accel_id);

static const struct of_device_id bmc150_accel_of_match[] = {
	{ .compatible = "bosch,bmc150_accel" },
	{ .compatible = "bosch,bmi055_accel" },
	{ .compatible = "bosch,bma255" },
	{ .compatible = "bosch,bma250e" },
	{ .compatible = "bosch,bma222e" },
	{ .compatible = "bosch,bma280" },
	{ },
};
MODULE_DEVICE_TABLE(of, bmc150_accel_of_match);

static struct i2c_driver bmc150_accel_driver = {
	.driver = {
		.name	= "bmc150_accel_i2c",
		.of_match_table = bmc150_accel_of_match,
		.acpi_match_table = ACPI_PTR(bmc150_accel_acpi_match),
		.pm	= &bmc150_accel_pm_ops,
	},
	.probe		= bmc150_accel_probe,
	.remove		= bmc150_accel_remove,
	.id_table	= bmc150_accel_id,
};
module_i2c_driver(bmc150_accel_driver);

MODULE_AUTHOR("Srinivas Pandruvada <srinivas.pandruvada@linux.intel.com>");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("BMC150 I2C accelerometer driver");
