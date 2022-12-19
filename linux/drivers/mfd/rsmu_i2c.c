// SPDX-License-Identifier: GPL-2.0+
/*
 * I2C driver for Renesas Synchronization Management Unit (SMU) devices.
 *
 * Copyright (C) 2021 Integrated Device Technology, Inc., a Renesas Company.
 */

#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/mfd/core.h>
#include <linux/mfd/rsmu.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#include "rsmu.h"

/*
 * 32-bit register address: the lower 8 bits of the register address come
 * from the offset addr byte and the upper 24 bits come from the page register.
 */
#define	RSMU_CM_PAGE_ADDR		0xFC
#define RSMU_CM_PAGE_MASK		0xFFFFFF00
#define RSMU_CM_ADDR_MASK		0x000000FF

/*
 * 15-bit register address: the lower 7 bits of the register address come
 * from the offset addr byte and the upper 8 bits come from the page register.
 */
#define	RSMU_SABRE_PAGE_ADDR		0x7F
#define	RSMU_SABRE_PAGE_WINDOW		128

#define RSMU_FC3_PAGE_ADDR		0xFC
#define RSMU_FC3_PAGE_MASK		0xFFFFFF00
#define RSMU_FC3_ADDR_MASK		0x000000FF

typedef int (*rsmu_rw_device)(struct rsmu_ddata *rsmu, u8 reg, u8 *buf, u8 bytes);

static const struct regmap_range_cfg rsmu_sabre_range_cfg[] = {
	{
		.range_min = 0,
		.range_max = 0x400,
		.selector_reg = RSMU_SABRE_PAGE_ADDR,
		.selector_mask = 0xFF,
		.selector_shift = 0,
		.window_start = 0,
		.window_len = RSMU_SABRE_PAGE_WINDOW,
	}
};

static bool rsmu_sabre_volatile_reg(struct device *dev, unsigned int reg)
{
	switch (reg) {
	case RSMU_SABRE_PAGE_ADDR:
		return false;
	default:
		return true;
	}
}

static int rsmu_smbus_i2c_write_device(struct rsmu_ddata *rsmu, u8 reg, u8 *buf, u8 bytes)
{
	struct i2c_client *client = to_i2c_client(rsmu->dev);

	return i2c_smbus_write_i2c_block_data(client, reg, bytes, buf);
}

static int rsmu_smbus_i2c_read_device(struct rsmu_ddata *rsmu, u8 reg, u8 *buf, u8 bytes)
{
	struct i2c_client *client = to_i2c_client(rsmu->dev);
	int ret;

	ret = i2c_smbus_read_i2c_block_data(client, reg, bytes, buf);
	if (ret == bytes)
		return 0;
	else if (ret < 0)
		return ret;
	else
		return -EIO;
}

static int rsmu_i2c_read_device(struct rsmu_ddata *rsmu, u8 reg, u8 *buf, u8 bytes)
{
	struct i2c_client *client = to_i2c_client(rsmu->dev);
	struct i2c_msg msg[2];
	int cnt;

	msg[0].addr = client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = &reg;

	msg[1].addr = client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = bytes;
	msg[1].buf = buf;

	cnt = i2c_transfer(client->adapter, msg, 2);

	if (cnt < 0) {
		dev_err(rsmu->dev, "i2c_transfer failed at addr: %04x!", reg);
		return cnt;
	} else if (cnt != 2) {
		dev_err(rsmu->dev,
			"i2c_transfer sent only %d of 2 messages", cnt);
		return -EIO;
	}

	return 0;
}

static int rsmu_i2c_write_device(struct rsmu_ddata *rsmu, u8 reg, u8 *buf, u8 bytes)
{
	struct i2c_client *client = to_i2c_client(rsmu->dev);
	/* we add 1 byte for device register */
	u8 msg[RSMU_MAX_WRITE_COUNT + 1];
	int cnt;

	if (bytes > RSMU_MAX_WRITE_COUNT)
		return -EINVAL;

	msg[0] = reg;
	memcpy(&msg[1], buf, bytes);

	cnt = i2c_master_send(client, msg, bytes + 1);

	if (cnt < 0) {
		dev_err(&client->dev,
			"i2c_master_send failed at addr: %04x!", reg);
		return cnt;
	}

	return 0;
}

static int rsmu_write_page_register(struct rsmu_ddata *rsmu, u32 reg,
				    rsmu_rw_device rsmu_write_device)
{
	u32 page;
	u8 page_reg_off;
	u8 buf[4];
	int err;

	switch (rsmu->type) {
	case RSMU_CM:
		/* Do not modify offset register for none-scsr registers */
		if (reg < RSMU_CM_SCSR_BASE)
			return 0;

		page = reg & RSMU_CM_PAGE_MASK;
		page_reg_off = RSMU_CM_PAGE_ADDR;
		break;
	case RSMU_FC3:
		page = reg & RSMU_FC3_PAGE_MASK;
		page_reg_off = RSMU_FC3_PAGE_ADDR;
		break;
	default:
		dev_err(rsmu->dev, "Unsupported RSMU device type: %d\n", rsmu->type);
		return -ENODEV;
	}

	/* Simply return if we are on the same page */
	if (rsmu->page == page)
		return 0;

	buf[0] = 0x0;
	buf[1] = (u8)((page >> 8) & 0xFF);
	buf[2] = (u8)((page >> 16) & 0xFF);
	buf[3] = (u8)((page >> 24) & 0xFF);

	err = rsmu_write_device(rsmu, page_reg_off, buf, sizeof(buf));
	if (err)
		dev_err(rsmu->dev, "Failed to set page offset 0x%x\n", page);
	else
		/* Remember the last page */
		rsmu->page = page;

	return err;
}

static int rsmu_i2c_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	struct rsmu_ddata *rsmu = i2c_get_clientdata((struct i2c_client *)context);
	u8 addr;
	int err;

	switch (rsmu->type) {
	case RSMU_CM:
		addr = (u8)(reg & RSMU_CM_ADDR_MASK);
		break;
	case RSMU_FC3:
		addr = (u8)(reg & RSMU_FC3_ADDR_MASK);
		break;
	default:
		dev_err(rsmu->dev, "Unsupported RSMU device type: %d\n", rsmu->type);
		return -ENODEV;
	}

	err = rsmu_write_page_register(rsmu, reg, rsmu_i2c_write_device);
	if (err)
		return err;

	err = rsmu_i2c_read_device(rsmu, addr, (u8 *)val, 1);
	if (err)
		dev_err(rsmu->dev, "Failed to read offset address 0x%x\n", addr);

	return err;
}

static int rsmu_i2c_reg_write(void *context, unsigned int reg, unsigned int val)
{
	struct rsmu_ddata *rsmu = i2c_get_clientdata((struct i2c_client *)context);
	u8 addr;
	u8 data = (u8)val;
	int err;

	switch (rsmu->type) {
	case RSMU_CM:
		addr = (u8)(reg & RSMU_CM_ADDR_MASK);
		break;
	case RSMU_FC3:
		addr = (u8)(reg & RSMU_FC3_ADDR_MASK);
		break;
	default:
		dev_err(rsmu->dev, "Unsupported RSMU device type: %d\n", rsmu->type);
		return -ENODEV;
	}

	err = rsmu_write_page_register(rsmu, reg, rsmu_i2c_write_device);
	if (err)
		return err;

	err = rsmu_i2c_write_device(rsmu, addr, &data, 1);
	if (err)
		dev_err(rsmu->dev,
			"Failed to write offset address 0x%x\n", addr);

	return err;
}

static int rsmu_smbus_i2c_reg_read(void *context, unsigned int reg, unsigned int *val)
{
	struct rsmu_ddata *rsmu = i2c_get_clientdata((struct i2c_client *)context);
	u8 addr;
	int err;

	switch (rsmu->type) {
	case RSMU_CM:
		addr = (u8)(reg & RSMU_CM_ADDR_MASK);
		break;
	case RSMU_FC3:
		addr = (u8)(reg & RSMU_FC3_ADDR_MASK);
		break;
	default:
		dev_err(rsmu->dev, "Unsupported RSMU device type: %d\n", rsmu->type);
		return -ENODEV;
	}

	err = rsmu_write_page_register(rsmu, reg, rsmu_smbus_i2c_write_device);
	if (err)
		return err;

	err = rsmu_smbus_i2c_read_device(rsmu, addr, (u8 *)val, 1);
	if (err)
		dev_err(rsmu->dev, "Failed to read offset address 0x%x\n", addr);

	return err;
}

static int rsmu_smbus_i2c_reg_write(void *context, unsigned int reg, unsigned int val)
{
	struct rsmu_ddata *rsmu = i2c_get_clientdata((struct i2c_client *)context);
	u8 addr;
	u8 data = (u8)val;
	int err;

	switch (rsmu->type) {
	case RSMU_CM:
		addr = (u8)(reg & RSMU_CM_ADDR_MASK);
		break;
	case RSMU_FC3:
		addr = (u8)(reg & RSMU_FC3_ADDR_MASK);
		break;
	default:
		dev_err(rsmu->dev, "Unsupported RSMU device type: %d\n", rsmu->type);
		return -ENODEV;
	}

	err = rsmu_write_page_register(rsmu, reg, rsmu_smbus_i2c_write_device);
	if (err)
		return err;

	err = rsmu_smbus_i2c_write_device(rsmu, addr, &data, 1);
	if (err)
		dev_err(rsmu->dev,
			"Failed to write offset address 0x%x\n", addr);

	return err;
}

static const struct regmap_config rsmu_cm_regmap_config = {
	.reg_bits = 32,
	.val_bits = 8,
	.max_register = 0x20120000,
	.reg_read = rsmu_i2c_reg_read,
	.reg_write = rsmu_i2c_reg_write,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config rsmu_smbus_cm_regmap_config = {
	.reg_bits = 32,
	.val_bits = 8,
	.max_register = 0x20120000,
	.reg_read = rsmu_smbus_i2c_reg_read,
	.reg_write = rsmu_smbus_i2c_reg_write,
	.cache_type = REGCACHE_NONE,
};

static const struct regmap_config rsmu_sabre_regmap_config = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = 0x400,
	.ranges = rsmu_sabre_range_cfg,
	.num_ranges = ARRAY_SIZE(rsmu_sabre_range_cfg),
	.volatile_reg = rsmu_sabre_volatile_reg,
	.cache_type = REGCACHE_RBTREE,
	.can_multi_write = true,
};

static const struct regmap_config rsmu_sl_regmap_config = {
	.reg_bits = 16,
	.val_bits = 8,
	.reg_format_endian = REGMAP_ENDIAN_BIG,
	.max_register = 0x340,
	.cache_type = REGCACHE_NONE,
	.can_multi_write = true,
};

static const struct regmap_config rsmu_fc3_regmap_config = {
	.reg_bits = 32,
	.val_bits = 8,
	.max_register = 0xFFF,
	.reg_read = rsmu_i2c_reg_read,
	.reg_write = rsmu_i2c_reg_write,
	.cache_type = REGCACHE_NONE,
};

static int rsmu_i2c_probe(struct i2c_client *client,
			  const struct i2c_device_id *id)
{
	const struct regmap_config *cfg;
	struct rsmu_ddata *rsmu;
	int ret;

	rsmu = devm_kzalloc(&client->dev, sizeof(*rsmu), GFP_KERNEL);
	if (!rsmu)
		return -ENOMEM;

	i2c_set_clientdata(client, rsmu);

	rsmu->dev = &client->dev;
	rsmu->type = (enum rsmu_type)id->driver_data;

	switch (rsmu->type) {
	case RSMU_CM:
		if (i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
			cfg = &rsmu_cm_regmap_config;
		} else if (i2c_check_functionality(client->adapter,
						   I2C_FUNC_SMBUS_I2C_BLOCK)) {
			cfg = &rsmu_smbus_cm_regmap_config;
		} else {
			dev_err(rsmu->dev, "Unsupported i2c adapter\n");
			return -ENOTSUPP;
		}
		break;
	case RSMU_SABRE:
		cfg = &rsmu_sabre_regmap_config;
		break;
	case RSMU_SL:
		cfg = &rsmu_sl_regmap_config;
		break;
	case RSMU_FC3:
		cfg = &rsmu_fc3_regmap_config;
		break;
	default:
		dev_err(rsmu->dev, "Unsupported RSMU device type: %d\n", rsmu->type);
		return -ENODEV;
	}

	if (rsmu->type == RSMU_CM)
		rsmu->regmap = devm_regmap_init(&client->dev, NULL, client, cfg);
	else
		rsmu->regmap = devm_regmap_init_i2c(client, cfg);

	if (IS_ERR(rsmu->regmap)) {
		ret = PTR_ERR(rsmu->regmap);
		dev_err(rsmu->dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	return rsmu_core_init(rsmu);
}

static int rsmu_i2c_remove(struct i2c_client *client)
{
	struct rsmu_ddata *rsmu = i2c_get_clientdata(client);

	rsmu_core_exit(rsmu);

	return 0;
}

static const struct i2c_device_id rsmu_i2c_id[] = {
	{ "8a34000",  RSMU_CM },
	{ "8a34001",  RSMU_CM },
	{ "82p33810", RSMU_SABRE },
	{ "82p33811", RSMU_SABRE },
	{ "8v19n850", RSMU_SL },
	{ "8v19n851", RSMU_SL },
	{ "rc32312", RSMU_FC3 },
	{ "rc32308", RSMU_FC3 },
	{}
};
MODULE_DEVICE_TABLE(i2c, rsmu_i2c_id);

static const struct of_device_id rsmu_i2c_of_match[] = {
	{ .compatible = "idt,8a34000",  .data = (void *)RSMU_CM },
	{ .compatible = "idt,8a34001",  .data = (void *)RSMU_CM },
	{ .compatible = "idt,82p33810", .data = (void *)RSMU_SABRE },
	{ .compatible = "idt,82p33811", .data = (void *)RSMU_SABRE },
	{ .compatible = "idt,8v19n850", .data = (void *)RSMU_SL },
	{ .compatible = "idt,8v19n851", .data = (void *)RSMU_SL },
	{ .compatible = "idt,rc32312", .data = (void *)RSMU_FC3 },
	{ .compatible = "idt,rc32308", .data = (void *)RSMU_FC3 },
	{}
};
MODULE_DEVICE_TABLE(of, rsmu_i2c_of_match);

static struct i2c_driver rsmu_i2c_driver = {
	.driver = {
		.name = "rsmu-i2c",
		.of_match_table = of_match_ptr(rsmu_i2c_of_match),
	},
	.probe = rsmu_i2c_probe,
	.remove	= rsmu_i2c_remove,
	.id_table = rsmu_i2c_id,
};

static int __init rsmu_i2c_init(void)
{
	return i2c_add_driver(&rsmu_i2c_driver);
}
subsys_initcall(rsmu_i2c_init);

static void __exit rsmu_i2c_exit(void)
{
	i2c_del_driver(&rsmu_i2c_driver);
}
module_exit(rsmu_i2c_exit);

MODULE_DESCRIPTION("Renesas SMU I2C driver");
MODULE_LICENSE("GPL");
