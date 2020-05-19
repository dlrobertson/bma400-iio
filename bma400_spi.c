// SPDX-License-Identifier: GPL-2.0-only
/*
 * SPI IIO driver for Bosch BMA400 triaxial acceleration sensor.
 *
 * Copyright 2020 Dan Robertson <dan@dlrobertson.com>
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/spi/spi.h>

#include <linux/spi/spi.h>
#include <linux/mod_devicetable.h>
#include <linux/module.h>
#include <linux/regmap.h>

#include "bma400.h"

static int bma400_regmap_spi_read(void *context,
				  const void *reg, size_t reg_size,
				  void *val, size_t val_size)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);
	ssize_t status;
	/*
	 * TODO(dlrobertson): What is a reasonable length to cap
	 * this at.
	 */
	u8 result[3];

	if (val_size > 3)
		return -ENOMEM;

	status = spi_write_then_read(spi, reg, 1, result, val_size + 1);
	if (status)
		return status;

	/*
	 * From the BMA400 datasheet:
	 *
	 * > For a basic read operation two bytes have to be read and the first
	 * > has to be dropped and the second byte must be interpreted.
	 */
	memcpy(val, result + 1, val_size);

	return 0;
}

static int bma400_regmap_spi_write(void *context, const void *data,
				   size_t count)
{
	struct device *dev = context;
	struct spi_device *spi = to_spi_device(dev);

	return spi_write(spi, data, count);
}

static struct regmap_bus bma400_regmap_bus = {
	.read = bma400_regmap_spi_read,
	.write = bma400_regmap_spi_write,
	.read_flag_mask = BIT(7),
	.max_raw_read = 2,
};

static int bma400_spi_probe(struct spi_device *spi)
{
	struct regmap *regmap;
	unsigned int val;
	int ret;

	regmap = devm_regmap_init(&spi->dev, &bma400_regmap_bus,
				  &spi->dev, &bma400_regmap_config);
	if (IS_ERR(regmap)) {
		dev_err(&spi->dev, "failed to create regmap\n");
		return PTR_ERR(regmap);
	}

	usleep_range(15000, 20000);

	/*
	 * Per the bma400 datasheet, the first SPI read may
	 * return garbage. The chip ID register will be read
	 * and checked again in the following probe.
	 */
	ret = regmap_read(regmap, BMA400_CHIP_ID_REG, &val);
	if (ret)
		dev_err(&spi->dev, "Failed to read chip id register\n");

	usleep_range(150000, 200000);

	return bma400_probe(&spi->dev, regmap, "bma400");
}

static int bma400_spi_remove(struct spi_device *spi)
{
	return bma400_remove(&spi->dev);
}

static const struct spi_device_id bma400_spi_ids[] = {
	{ "bma400", 0 },
	{ }
};
MODULE_DEVICE_TABLE(spi, bma400_spi_ids);

static const struct of_device_id bma400_of_spi_match[] = {
	{ .compatible = "bosch,bma400" },
	{ }
};
MODULE_DEVICE_TABLE(of, bma400_of_spi_match);

static struct spi_driver bma400_spi_driver = {
	.driver = {
		.name = "bma400",
		.of_match_table = bma400_of_spi_match,
	},
	.probe    = bma400_spi_probe,
	.remove   = bma400_spi_remove,
	.id_table = bma400_spi_ids,
};

module_spi_driver(bma400_spi_driver);

MODULE_AUTHOR("Dan Robertson <dan@dlrobertson.com>");
MODULE_DESCRIPTION("Bosch BMA400 triaxial acceleration sensor (SPI)");
MODULE_LICENSE("GPL");
