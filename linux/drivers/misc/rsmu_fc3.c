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
#include <asm/unaligned.h>

#include "rsmu_cdev.h"

#define FW_FILENAME	"rsmufc3.bin"

static int check_and_set_masks(struct rsmu_cdev *rsmu, u16 addr, u8 val)
{
	int err = 0;

	return err;
}

static int get_apll_reinit_reg_offset(enum fw_version fw_ver, u16 *apll_reinit_reg_offset)
{
	switch (fw_ver) {
	case V_DEFAULT:
	case VFC3W:
		*apll_reinit_reg_offset = SOFT_RESET_CTRL;
		break;
	case VFC3A:
		*apll_reinit_reg_offset = MISC_CTRL;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int read_device_id(struct rsmu_cdev *rsmu, u16 *device_id)
{
	int err;
	u8 buf[2] = {0};

	err = regmap_bulk_read(rsmu->regmap, DEVICE_ID,
			       &buf, sizeof(buf));
	if (err)
		return err;

	*device_id = get_unaligned_le16(buf);

	return 0;
}

static int hw_calibrate(struct rsmu_cdev *rsmu)
{
	int err = 0;
	u8 val;
	enum fw_version fw_ver = rsmu->fw_version;
	u16 apll_reinit_reg_addr;
	u8 apll_reinit_mask;

	err = get_apll_reinit_reg_offset(fw_ver, &apll_reinit_reg_addr);
	if (err)
		return err;
	apll_reinit_mask = IDTFC3_FW_FIELD(fw_ver, VFC3A, APLL_REINIT);

	/* Toggle TDC_DAC_RECAL_REQ:
		(1) set tdc_en to 1
		(2) set tdc_dac_recal_req to 0
		(3) set tdc_dac_recal_req to 1 */
	if (fw_ver == VFC3A) {
		val = TDC_EN;
		err = regmap_bulk_write(rsmu->regmap, TDC_ENABLE_CTRL,
					&val, sizeof(val));
		if (err)
			return err;
		val = 0;
		err = regmap_bulk_write(rsmu->regmap, TDC_DAC_CAL_CTRL,
					&val, sizeof(val));
		if (err)
			return err;
		val = TDC_DAC_RECAL_REQ_VFC3A;
		err = regmap_bulk_write(rsmu->regmap, TDC_DAC_CAL_CTRL,
					&val, sizeof(val));
		if (err)
			return err;
	} else {
		val = TDC_EN;
		err = regmap_bulk_write(rsmu->regmap, TDC_CTRL,
					&val, sizeof(val));
		if (err)
			return err;
		val = TDC_EN | TDC_DAC_RECAL_REQ;
		err = regmap_bulk_write(rsmu->regmap, TDC_CTRL,
					&val, sizeof(val));
		if (err)
			return err;
	}
	mdelay(10);

	/* Toggle APLL_REINIT:
		(1) set apll_reinit to 0
		(2) set apll_reinit to 1 */
	err = regmap_bulk_read(rsmu->regmap, apll_reinit_reg_addr,
			       &val, sizeof(val));
	val &= ~apll_reinit_mask;
	err = regmap_bulk_write(rsmu->regmap, apll_reinit_reg_addr,
			        &val, sizeof(val));
	if (err)
		return err;
	val |= apll_reinit_mask;
	err = regmap_bulk_write(rsmu->regmap, apll_reinit_reg_addr,
			        &val, sizeof(val));
	if (err)
		return err;
	mdelay(10);

	return err;	
}

static int get_losmon_sts_reg_offset(enum fw_version fw_ver, u8 clock_index, u16 *losmon_sts_reg_offset)
{
	switch (clock_index) {
	case 0:
		*losmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, LOSMON_STS_0);
		break;
	case 1:
		*losmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, LOSMON_STS_1);
		break;
	case 2:
		*losmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, LOSMON_STS_2);
		break;
	case 3:
		*losmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, LOSMON_STS_3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int get_freqmon_sts_reg_offset(enum fw_version fw_ver, u8 clock_index, u16 *freqmon_sts_reg_offset)
{
	switch (clock_index) {
	case 0:
		*freqmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, FREQMON_STS_0);
		break;
	case 1:
		*freqmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, FREQMON_STS_1);
		break;
	case 2:
		*freqmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, FREQMON_STS_2);
		break;
	case 3:
		*freqmon_sts_reg_offset = IDTFC3_FW_REG(fw_ver, VFC3A, FREQMON_STS_3);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int rsmu_fc3_get_dpll_state(struct rsmu_cdev *rsmu,
				   u8 dpll,
				   u8 *state)
{
	u16 reg_addr;
	u8 reg;
	int err;

	if (dpll > MAX_DPLL_INDEX) {
		return -EINVAL;
	}

	reg_addr = IDTFC3_FW_REG(rsmu->fw_version, VFC3A, DPLL_STS);
	if (rsmu->fw_version == VFC3A) {
		(void)dpll;
	} else {
		reg_addr += dpll * 0x100;
	}

	err = regmap_bulk_read(rsmu->regmap, reg_addr, &reg, sizeof(reg));
	if (err)
		return err;

	reg = (reg & DPLL_STATE_STS_MASK) >> DPLL_STATE_STS_SHIFT;

	switch (reg) {
	case DPLL_STATE_FREERUN:
	case DPLL_STATE_WRITE_FREQUENCY:
		*state = E_SRVLOUNQUALIFIEDSTATE;
		break;
	case DPLL_STATE_ACQUIRE:
	case DPLL_STATE_HITLESS_SWITCH:
		*state = E_SRVLOLOCKACQSTATE;
		break;
	case DPLL_STATE_LOCKED:
		*state = E_SRVLOTIMELOCKEDSTATE;
		break;
	case DPLL_STATE_HOLDOVER:
		*state = E_SRVLOHOLDOVERINSPECSTATE;
		break;
	default:
		*state = E_SRVLOSTATEINVALID;
		break;
	}

	return 0;
}

static int rsmu_fc3_get_fw_version(struct rsmu_cdev *rsmu)
{
	int err;
	u16 device_id;

	err = read_device_id(rsmu, &device_id);
	if (err) {
		rsmu->fw_version = V_DEFAULT;
		return err;
	}

	if (device_id & DEVICE_ID_MASK) {
		rsmu->fw_version = VFC3W;
		dev_info(rsmu->dev, "identified FC3W device\n");
	} else {
		rsmu->fw_version = VFC3A;
		dev_info(rsmu->dev, "identified FC3A device\n");
	}

	return 0;
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
	return hw_calibrate(rsmu);
}

static int rsmu_fc3_get_clock_index(struct rsmu_cdev *rsmu,
				    u8 dpll,
				    s8 *clock_index)
{
	u16 reg_addr;
	u8 reg;
	enum dpll_state dpll_state_sts;
	int err;

	if (dpll > MAX_DPLL_INDEX) {
		return -EINVAL;
	}

	reg_addr = IDTFC3_FW_REG(rsmu->fw_version, VFC3A, DPLL_STS);
	if (rsmu->fw_version == VFC3A) {
		(void)dpll;
	} else {
		reg_addr += dpll * 0x100;
	}

	err = regmap_bulk_read(rsmu->regmap, reg_addr, &reg, sizeof(reg));
	if (err)
		return err;

	dpll_state_sts = (enum dpll_state)((reg & DPLL_STATE_STS_MASK) >> DPLL_STATE_STS_SHIFT);
	if ((dpll_state_sts == DPLL_STATE_LOCKED) || (dpll_state_sts == DPLL_STATE_ACQUIRE) || (dpll_state_sts == DPLL_STATE_HITLESS_SWITCH)) {
		*clock_index = (reg & DPLL_REF_SEL_STS_MASK) >> DPLL_REF_SEL_STS_SHIFT;
	}
	else {
		*clock_index = -1;
	}

	return err;
}

static int rsmu_fc3_set_clock_priorities(struct rsmu_cdev *rsmu, u8 dpll, u8 number_entries,
					 struct rsmu_priority_entry *priority_entry)
{
	int priority_index;
	u16 reg = 0;
	u8 clock_index;
	u8 priority;
	u8 buf[2] = {0};
	int err;
	u16 reg_addr;

	if (dpll > MAX_DPLL_INDEX) {
		return -EINVAL;
	}

	reg_addr = DPLL_REF_PRIORITY_CNFG;
	if (rsmu->fw_version == VFC3A) {
		(void)dpll;
	} else {
		reg_addr += dpll * 0x100;
	}

	/* MAX_NUM_REF_PRIORITY is maximum number of priorities */
	if (number_entries > MAX_NUM_REF_PRIORITY)
		return -EINVAL;

	/* Disable clock priorities initially and then enable as needed in loop (dpll_refx_priority_disable[3:0]) */
	reg |= DPLL_REFX_PRIORITY_DISABLE_MASK;

	for (priority_index = 0; priority_index < number_entries; priority_index++) {
		clock_index = priority_entry->clock_index;
		priority = priority_entry->priority;

		if ((clock_index > MAX_REFERENCE_INDEX) || (priority > MAX_NUM_REF_PRIORITY))
			return -EINVAL;

		/* Set clock priority disable bit to zero to enable it */

		switch (clock_index) {
		case 0:
			reg = ((reg & (~DPLL_REF0_PRIORITY_ENABLE_AND_SET_MASK)) | (priority << DPLL_REF0_PRIORITY_SHIFT));
			break;
		case 1:
			reg = ((reg & (~DPLL_REF1_PRIORITY_ENABLE_AND_SET_MASK)) | (priority << DPLL_REF1_PRIORITY_SHIFT));
			break;
		case 2:
			reg = ((reg & (~DPLL_REF2_PRIORITY_ENABLE_AND_SET_MASK)) | (priority << DPLL_REF2_PRIORITY_SHIFT));
			break;
		case 3:
			reg = ((reg & (~DPLL_REF3_PRIORITY_ENABLE_AND_SET_MASK)) | (priority << DPLL_REF3_PRIORITY_SHIFT));
			break;
		default:
			return -EINVAL;
		}

		priority_entry++;
	}

	put_unaligned_le16(reg, buf);

	err = regmap_bulk_write(rsmu->regmap, DPLL_REF_PRIORITY_CNFG, &buf, sizeof(buf));

	if (err) {
		dev_err(rsmu->dev, "err\n");
	}

	return err;
}

static int rsmu_fc3_get_reference_monitor_status(struct rsmu_cdev *rsmu, u8 clock_index,
						 struct rsmu_reference_monitor_status_alarms *alarms)
{
	u16 losmon_sts_reg_addr;
	u16 freqmon_sts_reg_addr;
	u8 los_reg;
	u8 buf[4] = {0};
	u32 freq_reg;
	int err;

	if (clock_index > MAX_REFERENCE_INDEX)
		return -EINVAL;

	err = get_losmon_sts_reg_offset(rsmu->fw_version, clock_index, &losmon_sts_reg_addr);
	if (err)
		return err;

	err = get_freqmon_sts_reg_offset(rsmu->fw_version, clock_index, &freqmon_sts_reg_addr);
	if (err)
		return err;

	err = regmap_bulk_read(rsmu->regmap, losmon_sts_reg_addr,
			       &los_reg, sizeof(los_reg));
	if (err)
		return err;

	alarms->los = los_reg & LOS_STS_MASK;

	alarms->no_activity = 0;

	err = regmap_bulk_read(rsmu->regmap, freqmon_sts_reg_addr,
			       &buf, sizeof(buf));
	if (err)
		return err;

	freq_reg = get_unaligned_le32(buf);
	alarms->frequency_offset_limit = (freq_reg >> FREQ_FAIL_STS_SHIFT) & 1;

	return err;
}

struct rsmu_ops fc3_ops = {
	.type = RSMU_FC3,
	.set_combomode = NULL,
	.get_dpll_state = rsmu_fc3_get_dpll_state,
	.get_dpll_ffo = NULL,
	.set_holdover_mode = NULL,
	.set_output_tdc_go = NULL,
	.get_fw_version = rsmu_fc3_get_fw_version,
	.load_firmware = rsmu_fc3_load_firmware,
	.get_clock_index = rsmu_fc3_get_clock_index,
	.set_clock_priorities = rsmu_fc3_set_clock_priorities,
	.get_reference_monitor_status = rsmu_fc3_get_reference_monitor_status
};

