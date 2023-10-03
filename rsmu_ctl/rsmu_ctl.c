/*
 * @file rsmu_ctl.c
 * @brief Utility program to directly control and debug a rsmu-cdev.
 * This utility was based on phc_ctl from the linuxptp open source project.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
#include <errno.h>
#include <fcntl.h>
#include <float.h>
#include <signal.h>
#include <inttypes.h>
#include <limits.h>
#include <net/if.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <math.h>

#include <sys/time.h>
#include <stdint.h>

#include <stdbool.h>
#include <sys/queue.h>
#include <time.h>

#include "servo.h"
#include "clockadj.h"
#include "config.h"
#include "contain.h"
#include "interface.h"
#include "phc.h"
#include "print.h"
#include "ts2phc.h"
#include "version.h"
#include "../linux/include/uapi/linux/rsmu.h"

#define NSEC2SEC 1000000000.0

#define BASE_DECIMAL     0
#define BASE_HEXADECIMAL 16

#define MIN_DPLL_IDX 0
#define MAX_DPLL_IDX 7

#define MIN_CLOCK_INDEX 0
#define MAX_CLOCK_INDEX 15

#define MIN_OFFSET 0
#define MAX_OFFSET 0x20120000

#define MIN_COUNT 1
#define MAX_COUNT 255

#define MIN_NUMBER_ENTRIES 0
#define MAX_NUMBER_ENTRIES 19

#define MIN_PRIORITY 0
#define MAX_PRIORITY 18

#define MIN_MODE 0
#define MAX_MODE 3

#define MIN_ENABLE 0
#define MAX_ENABLE 1

#define MIN_TDC 0
#define MAX_TDC 3

#define MIN_WRITE_VAL 0
#define MAX_WRITE_VAL 255

#define MIN_TIME 0.0
#define MAX_TIME DBL_MAX

typedef int (*cmd_func_t)(int, int, char *[]);

struct cmd_t {
	const char *name;
	const cmd_func_t function;
};

struct ptp4l {
	clockid_t clkid;
	double period;
	int write_phase_mode;
	struct servo *servo;
	enum servo_state servo_state;	
};

static cmd_func_t get_command_function(const char *name);
static inline int name_is_a_command(const char *name);

/* Helper functions */

/* trap the alarm signal so that pause() will wake up on receipt */
static void handle_alarm(int s)
{
	return;
}

static void double_to_timespec(double d, struct timespec *ts)
{
	double fraction;
	double whole;

	fraction = modf(d, &whole);

	/* cast the whole value to a time_t to store as seconds */
	ts->tv_sec = (time_t)whole;
	/* tv_nsec is a long, so we multiply the nanoseconds per second double
	 * value by our fractional component. This results in a correct
	 * timespec from the double representing seconds.
	 */
	ts->tv_nsec = (long)(NSEC2SEC * fraction);
}

static int install_handler(int signum, void(*handler)(int))
{
	struct sigaction action;
	sigset_t mask;

	/* Unblock the signal */
	sigemptyset(&mask);
	sigaddset(&mask, signum);
	sigprocmask(SIG_UNBLOCK, &mask, NULL);

	/* Install the signal handler */
	action.sa_handler = handler;
	action.sa_flags = 0;
	sigemptyset(&action.sa_mask);
	sigaction(signum, &action, NULL);

	return 0;
}

static void usage(const char *progname)
{
	fprintf(stderr,
		"\n"
		"usage: %s [options] <device> -- [command]\n\n"
		" device         MISC device, ex. /dev/rsmu0"
		"\n"
		" options\n"
		" -l [num]       set the logging level to 'num'\n"
		" -q             do not print messages to the syslog\n"
		" -Q             do not print messages to stdout\n"
		" -v             prints the software version and exits\n"
		" -h             prints this message and exits\n"
		"\n"
		" commands\n"
		" specify commands with arguments. Can specify multiple\n"
		" commands to be executed in order.\n"
		"   get_current_clock_index <dpll_n>                get current clock index of <dpll_n>\n"
		"   get_ffo <dpll_n>                                get <dpll_n> FFO in ppb\n"
		"   get_reference_monitor_status <clock_n>          get reference monitor status of <clock_n>\n"
		"   get_state <dpll_n>                              get state of <dpll_n>\n"
		"   get_tdc_meas <mode 1/0> <cfg> <period(s)>       get a tdc measurement (FC3W only)\n"
                "                                                   mode 1: continuous, 0: one-shot\n"
		"   time_sync_ext <cfg> <period(s)>                 align time_sync with input signal (FC3W only)\n"
		"   rd <offset (hex)> [count]                       read [count] bytes from offset (by default [count] is 1)\n"
		"   set_clock_priorities <dpll_n> <num_entries>     set [clock_n] [priority_n] for <num_entries> of <dpll_n>\n"
		"                        [clock_n] [priority_n]\n"
		"   set_combo_mode <dpll_n> <mode>                  set combo mode\n"
		"                                                    CM\n"
		"                                                      0   Hold Disabled\n"
		"                                                      1-3 Hold Enabled\n"
		"                                                    Sabre\n"
		"                                                      0   PHASE_FREQ_OFFSET\n"
		"                                                      1   FREQ_OFFSET\n"
		"                                                      2   FAST_AVG_FREQ_OFFSET\n"
		"                                                      3   PLL_COMBO_MODE_HOLD\n"
		"  set_holdover_mode <dpll_n> <enable> <mode>       set holdover mode\n"
		"  set_output_tdc_go <tdc_n> <enable>               set output TDC go bit\n"
		"  wait <seconds>                                   pause <seconds> between commands\n"
		"  wr <offset (hex)> <count> <val (hex)>            write [count] bytes with <val> to <offset>\n"
		"\n",
		progname);
}

/* Argument handler functions */
static int handle_cfg_arg(const char *func_name, char *config, struct config **cfg)
{
	struct config *tmp;

	tmp = config_create();
	if (!tmp) {
		pr_err("%s: config_create failed", func_name);
		return -2;
	}

	if (config && config_read(config, tmp)) {
		pr_err("%s: config_read failed", func_name);
		config_destroy(tmp);
		return -2;
	}

	*cfg = tmp;
	return 0;
}

static int handle_meas_mode_arg(const char *func_name, char *mode, bool *continuous)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(mode, &tmp, 0, 1, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: meas mode '%s' is not valid.", func_name, mode);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: meas mode '%s' is not valid.", func_name, mode);
		return -2;
	default:
		pr_err("%s: couldn't process meas mode '%s'", func_name, mode);
		return -2;
	}

	if(tmp)
		*continuous = true;
	else
		*continuous = false;

	return 0;
}

static int handle_period_arg(const char *func_name, char *period_arg, double *period)
{
	enum parser_result r;
	double tmp;

	r = get_ranged_double(period_arg, &tmp, 0.1, 2);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: threshold '%s' is not a valid uint", func_name, period_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: threshold '%s' is out of range.", func_name, period_arg);
		return -2;
	default:
		pr_err("%s: couldn't process threshold '%s'", func_name, period_arg);
		return -2;
	}

	*period = tmp;

	return 0;
}

static int handle_dpll_arg(const char *func_name, char *dpll_arg, unsigned char *dpll)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(dpll_arg, &tmp, MIN_DPLL_IDX, MAX_DPLL_IDX, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: dpll '%s' is not a valid uint", func_name, dpll_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: dpll '%s' is out of range.", func_name, dpll_arg);
		return -2;
	default:
		pr_err("%s: couldn't process dpll '%s'", func_name, dpll_arg);
		return -2;
	}

	*dpll = (unsigned char)tmp;

	return 0;
}

static int handle_clock_index_arg(const char *func_name, char *clock_index_arg, unsigned char *clock_index)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(clock_index_arg, &tmp, MIN_CLOCK_INDEX, MAX_CLOCK_INDEX, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: clock index '%s' is not a valid uint", func_name, clock_index_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: clock index '%s' is out of range.", func_name, clock_index_arg);
		return -2;
	default:
		pr_err("%s: couldn't process clock index '%s'", func_name, clock_index_arg);
		return -2;
	}

	*clock_index = (unsigned char)tmp;

	return 0;
}

static int handle_offset_arg(const char *func_name, char *offset_arg, unsigned int *offset)
{
	enum parser_result r;

	r = get_ranged_uint(offset_arg, (unsigned int *)offset, MIN_OFFSET, MAX_OFFSET, BASE_HEXADECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: offset '%s' is not a valid uint", func_name, offset_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: offset '%s' is out of range.", func_name, offset_arg);
		return -2;
	default:
		pr_err("%s: couldn't process offset '%s'", func_name, offset_arg);
		return -2;
	}

	return 0;
}

static int handle_count_arg(const char *func_name, char *count_arg, unsigned char *count)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(count_arg, &tmp, MIN_COUNT, MAX_COUNT, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: count '%s' is not a valid uint", func_name, count_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: count '%s' is out of range.", func_name, count_arg);
		return -2;
	default:
		pr_err("%s: couldn't process count '%s'", func_name, count_arg);
		return -2;
	}

	*count = (unsigned char)tmp;

	return 0;
}

static int handle_num_entries_arg(const char *func_name, char *num_entries_arg, unsigned char *num_entries)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(num_entries_arg, &tmp, MIN_NUMBER_ENTRIES, MAX_NUMBER_ENTRIES, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: number of entries '%s' is not a valid uint", func_name, num_entries_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: number of entries '%s' is out of range.", func_name, num_entries_arg);
		return -2;
	default:
		pr_err("%s: couldn't process number of entries '%s'", func_name, num_entries_arg);
		return -2;
	}

	*num_entries = (unsigned char)tmp;

	return 0;
}

static int handle_pri_arg(const char *func_name, char *pri_arg, unsigned char *pri)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(pri_arg, &tmp, MIN_PRIORITY, MAX_PRIORITY, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: priority '%s' is not a valid uint", func_name, pri_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: priority '%s' is out of range.", func_name, pri_arg);
		return -2;
	default:
		pr_err("%s: couldn't process priority '%s'", func_name, pri_arg);
		return -2;
	}

	*pri = (unsigned char)tmp;

	return 0;
}

static int handle_mode_arg(const char *func_name, char *mode_arg, unsigned char *mode)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(mode_arg, &tmp, MIN_MODE, MAX_MODE, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: mode '%s' is not a valid uint", func_name, mode_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: mode '%s' is out of range.", func_name, mode_arg);
		return -2;
	default:
		pr_err("%s: couldn't process mode '%s'", func_name, mode_arg);
		return -2;
	}

	*mode = (unsigned char)tmp;

	return 0;
}

static int handle_enable_arg(const char *func_name, char *enable_arg, unsigned char *enable)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(enable_arg, &tmp, MIN_ENABLE, MAX_ENABLE, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: enable '%s' is not a valid uint", func_name, enable_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: enable '%s' is out of range.", func_name, enable_arg);
		return -2;
	default:
		pr_err("%s: couldn't process enable '%s'", func_name, enable_arg);
		return -2;
	}

	*enable = (unsigned char)tmp;

	return 0;
}

static int handle_tdc_arg(const char *func_name, char *tdc_arg, unsigned char *tdc)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(tdc_arg, &tmp, MIN_TDC, MAX_TDC, BASE_DECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: tdc '%s' is not a valid uint", func_name, tdc_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: tdc '%s' is out of range.", func_name, tdc_arg);
		return -2;
	default:
		pr_err("%s: couldn't process tdc '%s'", func_name, tdc_arg);
		return -2;
	}

	*tdc = (unsigned char)tmp;

	return 0;
}

static int handle_write_val_arg(const char *func_name, char *write_val_arg, unsigned char *write_val)
{
	enum parser_result r;
	unsigned int tmp;

	r = get_ranged_uint(write_val_arg, &tmp, MIN_WRITE_VAL, MAX_WRITE_VAL, BASE_HEXADECIMAL);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: write value '%s' is not a valid uint", func_name, write_val_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: write value '%s' is out of range.", func_name, write_val_arg);
		return -2;
	default:
		pr_err("%s: couldn't process write value '%s'", func_name, write_val_arg);
		return -2;
	}

	*write_val = (unsigned char)tmp;

	return 0;
}

static int handle_time_arg(const char *func_name, char *time_arg, double *time)
{
	enum parser_result r;

	r = get_ranged_double(time_arg, time, MIN_TIME, MAX_TIME);

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: time '%s' is not a valid uint", func_name, time_arg);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: time '%s' is out of range.", func_name, time_arg);
		return -2;
	default:
		pr_err("%s: couldn't process time '%s'", func_name, time_arg);
		return -2;
	}

	return 0;
}

/* Command functions */

static int do_get_current_clock_index(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_current_clock_index get = {0};
	int err;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll argument", __func__);
		return -2;
	}

	err = handle_dpll_arg(__func__, cmdv[0], &get.dpll);
	if (err) {
		return err;
	}

	if (ioctl(cdevFd, RSMU_GET_CURRENT_CLOCK_INDEX, &get)) {
		pr_err("%s: failed - is dpll %u valid for this part?", __func__, get.dpll);
		return -1;
	}

	printf("dpll %u: clock index = %d\n", get.dpll, get.clock_index);

	return 1;
}

static int do_get_ffo(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_get_ffo get = {0};
	int err;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll argument", __func__);
		return -2;
	}

	err = handle_dpll_arg(__func__, cmdv[0], &get.dpll);
	if (err) {
		return err;
	}

	if (ioctl(cdevFd, RSMU_GET_FFO, &get)) {
		pr_err("%s: failed - is dpll %u valid for this part?", __func__, get.dpll);
		return -1;
	}

	printf("dpll %u: ffo = %.6f ppb\n", get.dpll, get.ffo * 1e-9);

	return 1;
}

static int get_tdc_meas(int cdevFd, bool continuous, int64_t *offset, double period)
{
	struct rsmu_get_tdc_meas get = {0};
	uint32_t period_ns = period * NS_PER_SEC;

	get.continuous = continuous;

	if (ioctl(cdevFd, RSMU_GET_TDC_MEAS, &get)) {
		pr_err("%s: failed - is this a FC3W device?", __func__);
		return -1;
	}
	
	if ((get.offset == 0xbaddbadd) || (get.offset < 0)) {
		pr_err("%s: measurement %lld is not valid", __func__, get.offset);
		return -1;
	}

	if(get.offset >= period_ns) {
		get.offset = get.offset - period_ns;
	}
	else if(get.offset > period_ns / 2)
		get.offset = -(period_ns - get.offset);

	*offset = -get.offset;
	return 0;
}

static struct servo *rsmu_servo_create(struct ptp4l *ptp4l, struct config *cfg)
{
	enum servo_type type = config_get_int(cfg, NULL, "clock_servo");
	struct servo *servo;
	double fadj;
	int max_adj;

	fadj = clockadj_get_freq(ptp4l->clkid);

	max_adj = phc_max_adj(ptp4l->clkid);

	servo = servo_create(cfg, type, -fadj, max_adj, 0);
	if (!servo)
		return NULL;

	servo_sync_interval(servo, ptp4l->period);

	return servo;
}

static int phc_init(struct ptp4l *ptp4l, struct config *cfg)
{
	struct interface *iface = STAILQ_FIRST(&cfg->interfaces);

	ptp4l->clkid = phc_open(interface_name(iface));
	if (ptp4l->clkid == CLOCK_INVALID)
		return -1;

	return 0;
}

static int ptp4l_init(struct config *cfg, struct ptp4l *ptp4l, double period)
{
	int err;

	err = phc_init(ptp4l, cfg);
	if (err) {
		pr_err("phc_init: failed");
		return err;
	}	

	ptp4l->period = period;
	ptp4l->servo_state = SERVO_UNLOCKED;
	ptp4l->servo = rsmu_servo_create(ptp4l, cfg);
	if (ptp4l->servo == NULL) {
		pr_err("servo_create: failed");
		return -1;
	}

	ptp4l->write_phase_mode = config_get_int(cfg, NULL, "write_phase_mode");

	return 0;	
}

static void ptp4l_close(struct config *cfg, struct ptp4l *ptp4l)
{
	if(cfg)
		config_destroy(cfg);

	if(ptp4l->servo)
		servo_destroy(ptp4l->servo);

	phc_close(ptp4l->clkid);
}

static int synchronize_clock(int cdevFd, struct ptp4l *ptp4l)
{
	bool continuous = false;
	struct timespec ts;
	int64_t ts_ns;
	int64_t offset;
	double adj;
	int err;

	do {
		/* default to continuous mode */
		err = get_tdc_meas(cdevFd, continuous, &offset, ptp4l->period);
		if (err) {
			return err;
		}

		clock_gettime(CLOCK_MONOTONIC, &ts);
		ts_ns = tmv_to_nanoseconds(timespec_to_tmv(ts));

		adj = servo_sample(ptp4l->servo, offset, ts_ns,
				   1.0, &ptp4l->servo_state);

		pr_info("offset %10" PRId64 " s%d freq %+7.0f",
			offset, ptp4l->servo_state, adj);

		switch (ptp4l->servo_state) {
		case SERVO_UNLOCKED:
			continuous = false;
			break;
		case SERVO_JUMP:
			continuous = false;
			if (clockadj_set_freq(ptp4l->clkid, -adj)) {
				goto servo_unlock;
			}
			if (clockadj_step(ptp4l->clkid, -offset)) {
				goto servo_unlock;
			}
			/* Wait for time sync pulse to catch up */
			usleep(ptp4l->period * 1000000);
			break;
		case SERVO_LOCKED:
			continuous = true;
			if (clockadj_set_freq(ptp4l->clkid, -adj)) {
				goto servo_unlock;
			}
			break;
		case SERVO_LOCKED_STABLE:
			if (ptp4l->write_phase_mode) {
				if (clockadj_set_phase(ptp4l->clkid, -offset)) {
					goto servo_unlock;
				}
				adj = 0;
			} else {
				if (clockadj_set_freq(ptp4l->clkid, -adj)) {
					goto servo_unlock;
				}
			}
		}
		continue;

servo_unlock:
		servo_reset(ptp4l->servo);
		ptp4l->servo_state = SERVO_UNLOCKED;
	} while(true);

	return 0;
}

static int do_synchronize_clock(int cdevFd, int cmdc, char *cmdv[])
{
	struct config *cfg = NULL;
	struct ptp4l ptp4l = {0};
	double period;
	int err;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required config file", __func__);
		return -2;
	}

	err = handle_cfg_arg(__func__, cmdv[0], &cfg);
	if (err) {
		return err;
	}

	if (!name_is_a_command(cmdv[1])) {
		err = handle_period_arg(__func__, cmdv[1], &period);
		if (err) {
			period = 1;
		}
	}

	err = ptp4l_init(cfg, &ptp4l, period);
	if (err) {
		ptp4l_close(cfg, &ptp4l);
		return err;
	}

	err = synchronize_clock(cdevFd, &ptp4l);
	if (err) {
		ptp4l_close(cfg, &ptp4l);
		return err;
	}	
	
	ptp4l_close(cfg, &ptp4l);

	return 2;
}

static int do_get_tdc_meas(int cdevFd, int cmdc, char *cmdv[])
{
	int err;
	bool continuous;
	int64_t offset;
	double period;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing tdc meas mode argument", __func__);
		return -2;
	}

	err = handle_meas_mode_arg(__func__, cmdv[0], &continuous);
	if (err) {
		return err;
	}

	if (!name_is_a_command(cmdv[1])) {
		err = handle_period_arg(__func__, cmdv[1], &period);
		if (err) {
			period = 1;
		}
	}

	err = get_tdc_meas(cdevFd, continuous, &offset, period);
	if (err) {
		return -1;
	}

	printf("TDC measurement: offset = %ld ns\n", offset);
	return 2;
}

static int do_get_reference_monitor_status(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_reference_monitor_status get = {0};
	int err;


	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required clock index argument", __func__);
		return -2;
	}

	err = handle_clock_index_arg(__func__, cmdv[0], &get.clock_index);
	if (err) {
		return err;
	}

	if (ioctl(cdevFd, RSMU_GET_REFERENCE_MONITOR_STATUS, &get)) {
		pr_err("%s: failed - is clock index %u valid for this part?", __func__, get.clock_index);
		return -1;
	}

	printf("clock index %u: reference monitor status: los = %u no act = %u ffo limit = %u\n",
	       get.clock_index, get.alarms.los, get.alarms.no_activity, get.alarms.frequency_offset_limit);

	return 1;
}

static int do_get_state(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_get_state get = {0};
	int err;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll index argument", __func__);
		return -2;
	}

	err = handle_dpll_arg(__func__, cmdv[0], &get.dpll);
	if (err) {
		return err;
	}

	if (ioctl(cdevFd, RSMU_GET_STATE, &get)) {
		pr_err("%s: failed - is dpll %u valid for this part?", __func__, get.dpll);
		return -1;
	}

	printf("dpll %u: state = %u\n", get.dpll, get.state);

	return 1;
}

static int do_rd(int cdevFd, int cmdc, char *cmdv[])
{
	unsigned int aligned_offset = 0;
	unsigned int count_offset = 0;
	struct rsmu_reg_rw get = {0};
	unsigned char count = 1;
	int args_consumed = 0;
	unsigned int offset;
	int i;
	int err;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required offset argument", __func__);
		return -2;
	}

	/* Extend to access page registers and SYS RAM3 */
	err = handle_offset_arg(__func__, cmdv[0], &offset);
	if (err) {
		return err;
	}
	args_consumed++;
	get.offset = offset;

	if (cmdc > 1 && !name_is_a_command(cmdv[1])) {
		err = handle_count_arg(__func__, cmdv[1], &count);
		if (err) {
			return err;
		}
		args_consumed++;
	}
	get.byte_count = count;

	if (ioctl(cdevFd, RSMU_REG_READ, &get)) {
		pr_err("%s: RSMU_REG_READ failed", __func__);
		return -1;
	}

	if (1 == count) {
		printf("%04x: %02x\n", offset, get.bytes[0]);
	} else {
		aligned_offset = offset & 0xfff0;
		count_offset = offset & 0x000f;

		for (i = 0; i < count + count_offset; i++) {
			if (0 == (i % 16)) {
				printf("\n%04x: ", aligned_offset);
			}

			if (aligned_offset < (offset & 0xffff)) {
				printf("-- ");
			} else {
				printf("%02x ", get.bytes[i - count_offset]);
			}
			aligned_offset++;
		}
		printf("\n");
	}

	return args_consumed;
}

static int do_set_clock_priorities(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_clock_priorities set = {0};
	int args_consumed = 0;
	int err;
	int i;

	if (cmdc < 4 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll, number of entries, clock index, and/or priority entry arguments", __func__);
		return -2;
	}

	err = handle_dpll_arg(__func__, cmdv[0], &set.dpll);
	if (err) {
		return err;
	}
	args_consumed++;

	if (!name_is_a_command(cmdv[1])) {
		err = handle_num_entries_arg(__func__, cmdv[1], &set.num_entries);
		if (err) {
			return err;
		}

		if (set.num_entries != ((cmdc - 2) / 2)) {
			pr_err("%s: insufficient arguments", __func__);
			return -2;
		}
	}
	args_consumed++;

	i = 0;
	while (args_consumed < cmdc) {
		if (!name_is_a_command(cmdv[args_consumed])) {
			if (args_consumed & 1) {
				printf("pri = %s\n", cmdv[args_consumed]);
				err = handle_pri_arg(__func__, cmdv[args_consumed], &set.priority_entry[i].priority);
				i++;
			} else {
				printf("clk = %s\n", cmdv[args_consumed]);
				err = handle_clock_index_arg(__func__, cmdv[args_consumed], &set.priority_entry[i].clock_index);
			}

			if (err) {
				return err;
			}
			args_consumed++;
		}
	}

	if (ioctl(cdevFd, RSMU_SET_CLOCK_PRIORITIES, &set)) {
		pr_err("%s: failed - is dpll %u and number of entries %u valid for this part?", __func__, set.dpll, set.num_entries);
		return -1;
	}

	printf("dpll %u: number of entries = %u\n", set.dpll, set.num_entries);

	return args_consumed;
}

static int do_set_combo_mode(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_combomode set = {0};
	int args_consumed = 0;
	int err;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll and mode arguments", __func__);
		return -2;
	}

	err = handle_dpll_arg(__func__, cmdv[0], &set.dpll);
	if (err) {
		return err;
	}
	args_consumed++;

	if (!name_is_a_command(cmdv[1])) {
		err = handle_mode_arg(__func__, cmdv[1], &set.mode);
		if (err) {
			return err;
		}
		args_consumed++;
	}

	if (ioctl(cdevFd, RSMU_SET_COMBOMODE, &set)) {
		pr_err("%s: failed - is dpll %u and mode %u valid for this part?", __func__, set.dpll, set.mode);
		return -1;
	}

	printf("dpll %u: mode = %u\n", set.dpll, set.mode);

	return args_consumed;
}

static int do_set_holdover_mode(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_holdover_mode set = {0};
	int args_consumed = 0;
	int err;

	if (cmdc < 3 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll, enable, and mode arguments", __func__);
		return -2;
	}

	err = handle_dpll_arg(__func__, cmdv[0], &set.dpll);
	if (err) {
		return err;
	}
	args_consumed++;

	if (!name_is_a_command(cmdv[1])) {
		err = handle_enable_arg(__func__, cmdv[1], &set.enable);
		if (err) {
			return err;
		}
		args_consumed++;
	}

	if (!name_is_a_command(cmdv[2])) {
		err = handle_mode_arg(__func__, cmdv[1], &set.mode);
		if (err) {
			return err;
		}
		args_consumed++;
	}

	if (ioctl(cdevFd, RSMU_SET_HOLDOVER_MODE, &set)) {
		pr_err("%s: failed - is dpll %u valid for this part?", __func__, set.dpll);
		return -1;
	}

	return args_consumed;
}

static int do_set_output_tdc_go(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_set_output_tdc_go set = {0};
	int args_consumed = 0;
	int err;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required tdc and enable arguments", __func__);
		return -2;
	}

	err = handle_tdc_arg(__func__, cmdv[0], &set.tdc);
	if (err) {
		return -2;
	}

	if (!name_is_a_command(cmdv[1])) {
		err = handle_enable_arg(__func__, cmdv[1], &set.enable);
		if (err) {
			return err;
		}
		args_consumed++;
	}

	if (ioctl(cdevFd, RSMU_SET_OUTPUT_TDC_GO, &set)) {
		pr_err("%s: failed - is tdc %u valid for this part?", __func__, set.tdc);
		return -1;
	}

	return args_consumed;
}

static int do_wait(int fd, int cmdc, char *cmdv[])
{
	double time_arg;
	struct timespec ts;
	struct itimerval timer;
	int err;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required sleep duration argument\n", __func__);
		return -2;
	}

	memset(&timer, 0, sizeof(timer));

	/* parse the double time offset argument */
	err = handle_time_arg(__func__, cmdv[0], &time_arg);
	if (err) {
		return err;
	}

	double_to_timespec(time_arg, &ts);
	timer.it_value.tv_sec = ts.tv_sec;
	timer.it_value.tv_usec = ts.tv_nsec / 1000;
	setitimer(ITIMER_REAL, &timer, NULL);
	pause();

	/* the SIGALRM is already trapped during initialization, so we will
	 * wake up here once the alarm is handled.
	 */
	pr_notice( "process slept for %lf seconds\n", time_arg);

	return 1;
}

static int do_wr(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_reg_rw set = {0};
	unsigned char count = 1;
	int args_consumed = 0;
	int i;
	int err;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required offset and byte_value arguments", __func__);
		return -2;
	}

	/* Extend to access page registers and SYS RAM3 */
	err = handle_offset_arg(__func__, cmdv[0], &set.offset);
	if (err) {
		return err;
	}
	args_consumed++;

	if (cmdc > 1 && !name_is_a_command(cmdv[1])) {
		err = handle_count_arg(__func__, cmdv[1], &count);
		if (err) {
			return err;
		}
		args_consumed++;
	}
	set.byte_count = count;

	if (cmdc < count + 2) {
		pr_err("%s: Not enough arguments to satisfy byte count %d", __func__, count);
		return -3;
	}

	for (i = 0; i < count; i++) {
		if (name_is_a_command(cmdv[2 + i])) {
			pr_err("%s: byte %d is a command, %s", __func__, i, cmdv[2 + i]);
			return -2;
		}
		err = handle_write_val_arg(__func__, cmdv[2 + i], &set.bytes[i]);
		if (err) {
			return err;
		}
		args_consumed++;
	}

	if (ioctl(cdevFd, RSMU_REG_WRITE, &set)) {
		pr_err("%s: RSMU_REG_WRITE failed", __func__);
		return -1;
	}

	printf("wr %04x %d ", set.offset, set.byte_count );

	for (i = 0; i < count; i++) {
		printf("%02x ", set.bytes[i]);
	}
	printf("\n");

	return args_consumed;
}

static const struct cmd_t all_commands[] = {
	{ "get_current_clock_index", &do_get_current_clock_index },
	{ "get_ffo", &do_get_ffo },
	{ "get_reference_monitor_status", &do_get_reference_monitor_status },
	{ "get_state", &do_get_state },
	{ "get_tdc_meas", &do_get_tdc_meas },
	{ "time_sync_ext", &do_synchronize_clock },
	{ "rd", &do_rd },
	{ "set_clock_priorities", &do_set_clock_priorities },
	{ "set_combo_mode", &do_set_combo_mode },
	{ "set_holdover_mode", &do_set_holdover_mode },
	{ "set_output_tdc_go", &do_set_output_tdc_go },
	{ "wait", &do_wait },
	{ "wr", &do_wr },
	{ 0, 0 }
};

static cmd_func_t get_command_function(const char *name)
{
	cmd_func_t cmd = NULL;
	int i;

	for (i = 0; all_commands[i].name != NULL; i++) {
		if (!strncmp(name,
			     all_commands[i].name,
			     strlen(all_commands[i].name)))
			cmd = all_commands[i].function;
	}

	return cmd;
}

static inline int name_is_a_command(const char *name)
{
	return get_command_function(name) != NULL;
}

static int run_cmds(int fd, int cmdc, char *cmdv[])
{
	cmd_func_t action = NULL;
	int result = 0;
	int i = 0;

	while (i < cmdc) {
		char *arg = cmdv[i];

		/* increment now to remove the command argument */
		i++;

		action = get_command_function(arg);
		if (action)
			result = action(fd, cmdc - i, &cmdv[i]);
		else
			pr_err("unknown command %s.", arg);

		/* result is how many arguments were used up by the command,
		 * not including the ";". We will increment the loop counter
		 * to avoid processing the arguments as commands.
		 */
		if (result < 0)
			return result;
		else
			i += result;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	char *default_cmdv[] = { "get_state" };
	int print_level = LOG_INFO;
	const char *progname;
	int use_syslog = 1;
	int verbose = 1;
	int fd = -1;
	char **cmdv;
	int result;
	int cmdc;
	int c;

	install_handler(SIGALRM, handle_alarm);

	/* Process the command line arguments. */
	progname = strrchr(argv[0], '/');
	progname = progname ? 1+progname : argv[0];
	while (EOF != (c = getopt(argc, argv,
				  "l:qQvh"))) {
		switch (c) {
		case 'l':
			if (get_arg_val_i(c, optarg, &print_level,
					  PRINT_LEVEL_MIN, PRINT_LEVEL_MAX))
				return -1;
			break;
		case 'q':
			use_syslog = 0;
			break;
		case 'Q':
			verbose = 0;
			break;
		case 'v':
			version_show(stdout);
			return 0;
		case 'h':
			usage(progname);
			return 0;
		default:
			usage(progname);
			return -1;
		}
	}

	print_set_progname(progname);
	print_set_verbose(verbose);
	print_set_syslog(use_syslog);
	print_set_level(print_level);

	if ((argc - optind) < 1) {
		usage(progname);
		return -1;
	}

	if ((argc - optind) == 1) {
		cmdv = default_cmdv;
		cmdc = 1;
	} else {
		cmdv = &argv[optind + 1];
		cmdc = argc - optind - 1;
	}

	pr_info("Opening %s ...", argv[optind]);

	fd = open(argv[optind], O_WRONLY);
	if ( fd < 0 ) {
		perror("Open failed");
		return -1;
	}

	/* pass the remaining arguments to the run_cmds loop */
	result = run_cmds(fd, cmdc, cmdv);
	close(fd);

	if (result < -1) {
		/* show usage when command fails */
		usage(progname);
		return result;
	}

	return 0;
}
