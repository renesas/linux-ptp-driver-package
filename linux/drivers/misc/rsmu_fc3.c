// SPDX-License-Identifier: GPL-2.0+
/*
 * This driver is developed for RC38xxx (FemtoClock3) series of
 * timing and synchronization devices.
 *
 * Copyright (C) 2023 Integrated Device Technology, Inc., a Renesas Company.
 */
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/mfd/idtRC38xxx_reg.h>
#include <linux/mfd/rsmu.h>
#include <uapi/linux/rsmu.h>
#include <asm/unaligned.h>

#include "rsmu_cdev.h"

#define FW_FILENAME	"rsmufc3.bin"

static int check_and_set_masks(struct rsmu_cdev *rsmu, u16 addr, u8 val)
{
	int err = 0;

	return err;
}

static int rsmu_fc3_load_firmware(struct rsmu_cdev *rsmu,
				  char fwname[FW_NAME_LEN_MAX])
{
	char fname[128] = FW_FILENAME;
	const struct firmware *fw;
	struct idtfc3_fwrc *rec;
	u16 addr;
	u8 val;
	int err;
	s32 len;

	if (fwname) /* module parameter */
		snprintf(fname, sizeof(fname), "%s", fwname);

	dev_info(rsmu->dev, "requesting firmware '%s'\n", fname);

	err = request_firmware(&fw, fname, rsmu->dev);

	if (err) {
		dev_err(rsmu->dev,
			"requesting firmware failed with err %d!\n", err);
		return err;
	}

	dev_dbg(rsmu->dev, "firmware size %zu bytes\n", fw->size);

	rec = (struct idtfc3_fwrc *) fw->data;

	for (len = fw->size; len > 0; len -= sizeof(*rec)) {
		if (rec->reserved) {
			dev_err(rsmu->dev,
				"bad firmware, reserved field non-zero\n");
			err = -EINVAL;
		} else {
			val = rec->value;
			addr = rec->hiaddr << 8 | rec->loaddr;

			rec++;

			err = check_and_set_masks(rsmu, addr, val);
		}

		if (err != -EINVAL) {
			err = 0;

			/* Max register */
			if (addr > 0xE88 )
				continue;

			err = regmap_bulk_write(rsmu->regmap, addr,
						&val, sizeof(val));
		}

		if (err)
			goto out;
	}

out:
	release_firmware(fw);
	return err;	
}

struct rsmu_ops fc3_ops = {
	.type = RSMU_FC3,
	.load_firmware = rsmu_fc3_load_firmware,
};

