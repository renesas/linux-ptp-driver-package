/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Core interface for Renesas Synchronization Management Unit (SMU) devices.
 *
 * Copyright (C) 2021 Integrated Device Technology, Inc., a Renesas Company.
 */

#ifndef __LINUX_MFD_RSMU_H
#define __LINUX_MFD_RSMU_H

#define RSMU_MAX_WRITE_COUNT		(255)
#define RSMU_MAX_READ_COUNT		(255)

/* The supported devices are ClockMatrix, Sabre, SnowLotus, and FemtoClock3 */
enum rsmu_type {
	RSMU_CM		= 0x34000, /* ClockMatrix */
	RSMU_SABRE	= 0x33810, /* Sabre */
	RSMU_SL		= 0x19850, /* SnowLotus */
	RSMU_FC3	= 0x32300, /* FemtoClock3 */
};

/**
 *
 * struct rsmu_ddata - device data structure for sub devices.
 *
 * @dev:    i2c/spi device.
 * @regmap: i2c/spi bus access.
 * @lock:   mutex used by sub devices to make sure a series of
 *          bus access requests are not interrupted.
 * @type:   RSMU device type.
 * @page:   i2c/spi bus driver internal use only.
 */
struct rsmu_ddata {
	struct device *dev;
	struct regmap *regmap;
	struct mutex lock;
	enum rsmu_type type;
	u32 page;
};
#endif /*  __LINUX_MFD_RSMU_H */
