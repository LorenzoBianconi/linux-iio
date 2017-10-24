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
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/iio/trigger.h>
#include <linux/iio/trigger_consumer.h>
#include <linux/iio/triggered_buffer.h>
#include <linux/iio/buffer.h>

#include "st_uvis25.h"

#define ST_UVIS25_REG_CTRL3_ADDR	0x22
#define ST_UVIS25_REG_HL_MASK		BIT(7)
#define ST_UVIS25_REG_STATUS_ADDR	0x27
#define ST_UVIS25_REG_UV_DA_MASK	BIT(0)

static irqreturn_t st_uvis25_trigger_handler_thread(int irq, void *private)
{
	struct st_uvis25_hw *hw = private;
	u8 status;
	int err;

	err = hw->tf->read(hw->dev, ST_UVIS25_REG_STATUS_ADDR, sizeof(status),
			   &status);
	if (err < 0)
		return IRQ_HANDLED;

	if (!(status & ST_UVIS25_REG_UV_DA_MASK))
		return IRQ_NONE;

	iio_trigger_poll_chained(hw->trig);

	return IRQ_HANDLED;
}

int st_uvis25_allocate_trigger(struct iio_dev *iio_dev)
{
	struct st_uvis25_hw *hw = iio_priv(iio_dev);
	bool irq_active_low = false;
	unsigned long irq_type;
	int err;

	irq_type = irqd_get_trigger_type(irq_get_irq_data(hw->irq));

	switch (irq_type) {
	case IRQF_TRIGGER_HIGH:
	case IRQF_TRIGGER_RISING:
		break;
	case IRQF_TRIGGER_LOW:
	case IRQF_TRIGGER_FALLING:
		irq_active_low = true;
		break;
	default:
		dev_info(hw->dev,
			 "mode %lx unsupported, using IRQF_TRIGGER_RISING\n",
			 irq_type);
		irq_type = IRQF_TRIGGER_RISING;
		break;
	}

	err = st_uvis25_write_with_mask(hw, ST_UVIS25_REG_CTRL3_ADDR,
					ST_UVIS25_REG_HL_MASK, irq_active_low);
	if (err < 0)
		return err;

	err = devm_request_threaded_irq(hw->dev, hw->irq, NULL,
					st_uvis25_trigger_handler_thread,
					irq_type | IRQF_ONESHOT,
					iio_dev->name, hw);
	if (err) {
		dev_err(hw->dev, "failed to request trigger irq %d\n",
			hw->irq);
		return err;
	}

	hw->trig = devm_iio_trigger_alloc(hw->dev, "%s-trigger",
					  iio_dev->name);
	if (!hw->trig)
		return -ENOMEM;

	iio_trigger_set_drvdata(hw->trig, iio_dev);
	hw->trig->dev.parent = hw->dev;

	return devm_iio_trigger_register(hw->dev, hw->trig);
}

static int st_uvis25_buffer_preenable(struct iio_dev *iio_dev)
{
	return st_uvis25_set_enable(iio_priv(iio_dev), true);
}

static int st_uvis25_buffer_postdisable(struct iio_dev *iio_dev)
{
	return st_uvis25_set_enable(iio_priv(iio_dev), false);
}

static const struct iio_buffer_setup_ops st_uvis25_buffer_ops = {
	.preenable = st_uvis25_buffer_preenable,
	.postenable = iio_triggered_buffer_postenable,
	.predisable = iio_triggered_buffer_predisable,
	.postdisable = st_uvis25_buffer_postdisable,
};

static irqreturn_t st_uvis25_buffer_handler_thread(int irq, void *p)
{
	u8 buffer[ALIGN(sizeof(u8), sizeof(s64)) + sizeof(s64)];
	struct iio_poll_func *pf = p;
	struct iio_dev *iio_dev = pf->indio_dev;
	struct st_uvis25_hw *hw = iio_priv(iio_dev);
	int err;

	err = hw->tf->read(hw->dev, iio_dev->channels[0].address, sizeof(u8),
			   buffer);
	if (err < 0)
		goto out;

	iio_push_to_buffers_with_timestamp(iio_dev, buffer,
					   iio_get_time_ns(iio_dev));

out:
	iio_trigger_notify_done(hw->trig);

	return IRQ_HANDLED;
}

int st_uvis25_allocate_buffer(struct iio_dev *iio_dev)
{
	struct st_uvis25_hw *hw = iio_priv(iio_dev);

	return devm_iio_triggered_buffer_setup(hw->dev, iio_dev, NULL,
					       st_uvis25_buffer_handler_thread,
					       &st_uvis25_buffer_ops);
}

MODULE_AUTHOR("Lorenzo Bianconi <lorenzo.bianconi@st.com>");
MODULE_DESCRIPTION("STMicroelectronics uvis25 buffer driver");
MODULE_LICENSE("GPL v2");
