/* SPDX-License-Identifier: GPL-2.0+ */
/*
 * Register Map - Based on PolarBear_CSRs.RevA.xlsx (2023-04-21)
 *
 * Copyright (C) 2023 Integrated Device Technology, Inc., a Renesas Company.
 */
#ifndef HAVE_IDTRC38XXX_REG
#define HAVE_IDTRC38XXX_REG

/* GLOBAL */
#define SOFT_RESET_CTRL               (0x15)
#define SOFT_RESET	BIT(0)
#define APLL_REINIT	BIT(1)

/* FOD */
#define FOD_0                         (0x300)
#define FOD_1                         (0x340)
#define FOD_2                         (0x380)
#define FOD_WRITE_FREQ_CTRL           (0x28)

/* TDCAPLL */
#define TDC_CTRL                      (0x44A)
#define TDC_EN			BIT(0)
#define TDC_DAC_RECAL_REQ	BIT(1)

#define TDC_FB_DIV_INT_CNFG           (0x442)
#define TDC_FB_DIV_INT_MASK	GENMASK(7, 0)
#define TDC_REF_DIV_CNFG              (0x443)
#define TDC_REF_DIV_MASK	GENMASK(2, 0)

/* TIME SYNC CHANNEL */
#define TIME_CLOCK_SRC                (0xA01)
#define TOD_COUNTER_READ_REQ          (0xA5F)
#define TOD_COUNTER_STS               (0xA60)
#define TOD_SYNC_LOAD_VAL_CTRL        (0xA10)
#define SYNC_COUNTER_MASK	GENMASK(51, 0)
#define SUB_SYNC_COUNTER_MASK	GENMASK(30, 0)

#define TOD_SYNC_LOAD_EN_CTRL         (0xA20)
#define TOD_SYNC_LOAD_REQ_CTRL        (0xA21)
#define SYNC_LOAD_ENABLE	BIT(1)
#define SUB_SYNC_LOAD_ENABLE	BIT(0)
#define SYNC_LOAD_REQ		BIT(0)

#define LPF_MODE_CNFG                 (0xA80)
enum lpf_mode {
	LPF_DISABLED = 0,
	LPF_WP = 1,
	LPF_HOLDOVER = 2,
	LPF_WF = 3,
	LPF_INVALID = 4,
};
#define LPF_CTRL                      (0xA98)
#define LPF_ENABLE		BIT(0)

#define LPF_WR_PHASE_CTRL             (0xAA8)
#define LPF_PCW			GENMASK(27, 0)
#define LPF_WR_FREQ_CTRL              (0xAB0)
#define LPF_FCW			GENMASK(32, 0)

/* firmware interface */
#define TIME_CLK_FREQ_ADDR            (0xFFA0)
#define XTAL_FREQ_ADDR                (0xFFA1)

/* XTAL_FREQ_ADDR / TIME_CLK_FREQ_ADDR */
enum {
	FREQ_MIN = 0,
	FREQ_25M = 1,
	FREQ_49_152M = 2,
	FREQ_50M = 3,
	FREQ_100M = 4,
	FREQ_125M = 5,
	FREQ_250M = 6,
	FREQ_MAX,
};

struct idtfc3_fwrc {
	u8 hiaddr;
	u8 loaddr;
	u8 value;
	u8 reserved;
} __packed;

#endif
