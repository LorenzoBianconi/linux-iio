/*
 * STMicroelectronics uvis25 sensor driver
 *
 * Copyright 2017 STMicroelectronics Inc.
 *
 * Lorenzo Bianconi <lorenzo.bianconi@st.com>
 *
 * Licensed under the GPL-2.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/iio/sysfs.h>
#include <linux/delay.h>
#include <linux/pm.h>
#include <linux/interrupt.h>

#include "st_uvis25.h"

#define ST_UVIS25_REG_WHOAMI_ADDR	0x0f
#define ST_UVIS25_REG_WHOAMI_VAL	0xca
#define ST_UVIS25_REG_CTRL1_ADDR	0x20
#define ST_UVIS25_REG_ODR_MASK		BIT(0)
#define ST_UVIS25_REG_BDU_MASK		BIT(1)
#define ST_UVIS25_REG_CTRL2_ADDR	0x21
#define ST_UVIS25_REG_BOOT_MASK		BIT(7)

static const struct iio_chan_spec st_uvis25_channels[] = {
	{
		.type = IIO_UVINDEX,
		.address = 0x28,
		.info_mask_separate = BIT(IIO_CHAN_INFO_PROCESSED),
		.scan_index = 0,
		.scan_type = {
			.sign = 'u',
			.realbits = 8,
			.storagebits = 8,
		},
	},
	IIO_CHAN_SOFT_TIMESTAMP(1),
};

static int st_uvis25_check_whoami(struct st_uvis25_hw *hw)
{
	u8 data;
	int err;

	err = hw->tf->read(hw->dev, ST_UVIS25_REG_WHOAMI_ADDR, sizeof(data),
			   &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read whoami register\n");
		return err;
	}

	if (data != ST_UVIS25_REG_WHOAMI_VAL) {
		dev_err(hw->dev, "wrong whoami {%02x vs %02x}\n",
			data, ST_UVIS25_REG_WHOAMI_VAL);
		return -ENODEV;
	}

	return 0;
}

int st_uvis25_write_with_mask(struct st_uvis25_hw *hw, u8 addr, u8 mask, u8 val)
{
	u8 data;
	int err;

	mutex_lock(&hw->lock);

	err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
	if (err < 0) {
		dev_err(hw->dev, "failed to read %02x register\n", addr);
		goto unlock;
	}

	data = (data & ~mask) | ((val << __ffs(mask)) & mask);

	err = hw->tf->write(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		dev_err(hw->dev, "failed to write %02x register\n", addr);

unlock:
	mutex_unlock(&hw->lock);

	return err < 0 ? err : 0;
}

int st_uvis25_set_enable(struct st_uvis25_hw *hw, bool enable)
{
	int err;

	err = st_uvis25_write_with_mask(hw, ST_UVIS25_REG_CTRL1_ADDR,
					ST_UVIS25_REG_ODR_MASK, enable);
	if (err < 0)
		return err;

	hw->enabled = enable;

	return 0;
}

static int st_uvis25_read_oneshot(struct st_uvis25_hw *hw, u8 addr, int *val)
{
	u8 data;
	int err;

	err = st_uvis25_set_enable(hw, true);
	if (err < 0)
		return err;

	msleep(1500);

	err = hw->tf->read(hw->dev, addr, sizeof(data), &data);
	if (err < 0)
		return err;

	st_uvis25_set_enable(hw, false);

	*val = data;

	return IIO_VAL_INT;
}

static int st_uvis25_read_raw(struct iio_dev *iio_dev,
			      struct iio_chan_spec const *ch,
			      int *val, int *val2, long mask)
{
	int ret;

	ret = iio_device_claim_direct_mode(iio_dev);
	if (ret)
		return ret;

	switch (mask) {
	case IIO_CHAN_INFO_PROCESSED: {
		struct st_uvis25_hw *hw = iio_priv(iio_dev);

		/*
		 * mask irq line during oneshot read since the sensor
		 * does not export the capability to disable data-ready line
		 * in the register map and it is enabled by default.
		 * If the line is unmasked during read_raw() it will be set
		 * active and never reset since the trigger is disabled
		 */
		if (hw->irq > 0)
			disable_irq(hw->irq);
		ret = st_uvis25_read_oneshot(hw, ch->address, val);
		if (hw->irq > 0)
			enable_irq(hw->irq);
		break;
	}
	default:
		ret = -EINVAL;
		break;
	}

	iio_device_release_direct_mode(iio_dev);

	return ret;
}

static const struct iio_info st_uvis25_info = {
	.read_raw = st_uvis25_read_raw,
};

static const unsigned long st_uvis25_scan_masks[] = { 0x1, 0x0 };

static int st_uvis25_init_sensor(struct st_uvis25_hw *hw)
{
	int err;

	err = st_uvis25_write_with_mask(hw, ST_UVIS25_REG_CTRL2_ADDR,
					ST_UVIS25_REG_BOOT_MASK, 1);
	if (err < 0)
		return err;

	msleep(2000);

	return st_uvis25_write_with_mask(hw, ST_UVIS25_REG_CTRL1_ADDR,
					 ST_UVIS25_REG_BDU_MASK, 1);
}

int st_uvis25_probe(struct device *dev, int irq,
		    const struct st_uvis25_transfer_function *tf_ops)
{
	struct st_uvis25_hw *hw;
	struct iio_dev *iio_dev;
	int err;

	iio_dev = devm_iio_device_alloc(dev, sizeof(*hw));
	if (!iio_dev)
		return -ENOMEM;

	dev_set_drvdata(dev, (void *)iio_dev);

	hw = iio_priv(iio_dev);
	hw->dev = dev;
	hw->irq = irq;
	hw->tf = tf_ops;

	mutex_init(&hw->lock);

	err = st_uvis25_check_whoami(hw);
	if (err < 0)
		return err;

	iio_dev->modes = INDIO_DIRECT_MODE;
	iio_dev->dev.parent = hw->dev;
	iio_dev->available_scan_masks = st_uvis25_scan_masks;
	iio_dev->channels = st_uvis25_channels;
	iio_dev->num_channels = ARRAY_SIZE(st_uvis25_channels);
	iio_dev->name = ST_UVIS25_DEV_NAME;
	iio_dev->info = &st_uvis25_info;

	err = st_uvis25_init_sensor(hw);
	if (err < 0)
		return err;

	if (hw->irq > 0) {
		err = st_uvis25_allocate_buffer(iio_dev);
		if (err < 0)
			return err;

		err = st_uvis25_allocate_trigger(iio_dev);
		if (err)
			return err;
	}

	return devm_iio_device_register(hw->dev, iio_dev);
}
EXPORT_SYMBOL(st_uvis25_probe);

static int __maybe_unused st_uvis25_suspend(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_uvis25_hw *hw = iio_priv(iio_dev);

	return st_uvis25_write_with_mask(hw, ST_UVIS25_REG_CTRL1_ADDR,
					 ST_UVIS25_REG_ODR_MASK, 0);
}

static int __maybe_unused st_uvis25_resume(struct device *dev)
{
	struct iio_dev *iio_dev = dev_get_drvdata(dev);
	struct st_uvis25_hw *hw = iio_priv(iio_dev);
	int err = 0;

	if (hw->enabled)
		err = st_uvis25_write_with_mask(hw, ST_UVIS25_REG_CTRL1_ADDR,
						ST_UVIS25_REG_ODR_MASK, 1);

	return err;
}

const struct dev_pm_ops st_uvis25_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(st_uvis25_suspend, st_uvis25_resume)
};
EXPORT_SYMBOL(st_uvis25_pm_ops);

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uvis25 sensor driver");
MODULE_LICENSE("GPL v2");
