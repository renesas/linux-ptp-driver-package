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

#include "print.h"
#include "version.h"
#include "../linux/include/uapi/linux/rsmu.h"

#define NSEC2SEC 1000000000.0

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
		"  get_ffo   <dpll_n>                              get <dpll_n> FFO in ppb\n"
		"  get_state <dpll_n>                              get state of <dpll_n>\n"
		"  rd        <offset (hex)> [count]                read [count] bytes from offset (default count 1)\n"
		"  set_combo_mode  <dpll_n> <mode>                 set combo mode\n"
		"                                                    CM\n"
		"                                                      0   Hold Disabled\n"
		"                                                      1-3 Hold Enabled\n"
		"                                                    Sabre\n"
		"                                                      0   PHASE_FREQ_OFFSET\n"
		"                                                      1   FREQ_OFFSET\n"
		"                                                      2   FAST_AVG_FREQ_OFFSET\n"
		"                                                      3   PLL_COMBO_MODE_HOLD\n"
		"  set_holdover_mode <dpll_n> <enable> <mode>      set holdover mode\n"
		"  set_output_tdc_go <tdc_n> <enable>              set output TDC go bit\n"
		"  wait      <seconds>                             pause <seconds> between commands\n"
		"  wr        <offset (hex)> <count> <val (hex)>    write <val> to <offset>\n"
		"\n",
		progname);
}

typedef int (*cmd_func_t)(int, int, char *[]);
struct cmd_t {
	const char *name;
	const cmd_func_t function;
};

static cmd_func_t get_command_function(const char *name);
static inline int name_is_a_command(const char *name);

static int do_get_ffo(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_get_ffo get = {0};
	enum parser_result r;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("get_ffo: missing required DPLL index argument");
		return -2;
	}

	r = get_ranged_uint(cmdv[0], (unsigned int *)&get.dpll, 0, 7, 0);
	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: dpll '%s' is not a valid uint", __func__, cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: dpll '%s' is out of range.", __func__, cmdv[0]);
		return -2;
	default:
		pr_err("%s: couldn't process dpll '%s'", __func__, cmdv[0]);
	}

	if (ioctl(cdevFd, RSMU_GET_FFO, &get)) {
		pr_err("%s: failed - is dpll%d valid for this part?", __func__, get.dpll);
		return -1;
	}

	printf("DPLL %d:  ffo =  %.6f ppb\n", get.dpll, get.ffo * 1e-9);

	return 1;
}

static int do_get_state(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_get_state get = {0};
	enum parser_result r;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("get_state: missing required DPLL index argument");
		return -2;
	}

	r = get_ranged_uint(cmdv[0], (unsigned int *)&get.dpll, 0, 7, 0);
	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: dpll '%s' is not a valid uint", __func__, cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: dpll '%s' is out of range.", __func__, cmdv[0]);
		return -2;
	default:
		pr_err("%s: couldn't process dpll '%s'", __func__, cmdv[0]);
		return -2;
	}

	if (ioctl(cdevFd, RSMU_GET_STATE, &get)) {
		pr_err("%s: failed - is dpll%d valid for this part?", __func__, get.dpll);
		return -1;
	}

	printf("DPLL %d:  state = %d\n", get.dpll, get.state);

	return 1;
}

static int do_rd(int cdevFd, int cmdc, char *cmdv[])
{
	unsigned int aligned_offset = 0;
	unsigned int count_offset = 0;
	struct rsmu_reg_rw get = {0};
	unsigned int count = 1;
	int args_consumed = 1;
	unsigned int offset;
	enum parser_result r;
	int i;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("rd: missing required offset argument");
		return -2;
	}

	/* Extend to access page registers and SYS RAM3 */
	r = get_ranged_uint(cmdv[0], &offset, 0, 0x20120000, 16);
	get.offset = offset;

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("rd: offset '%s' is not a valid uint", cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("rd: offset '%s' is out of range.", cmdv[0]);
		return -2;
	default:
		pr_err("rd: couldn't process '%s' for offset", cmdv[0]);
		return -2;
	}

	if (cmdc > 1 && !name_is_a_command(cmdv[1])) {
		r = get_ranged_uint(cmdv[1], &count, 1, 256, 0);
		switch (r) {
		case PARSED_OK:
			break;
		case MALFORMED:
			pr_err("rd: count '%s' is not a valid uint", cmdv[1]);
			return -2;
		case OUT_OF_RANGE:
			pr_err("rd: count '%s' is out of range.", cmdv[1]);
			return -2;
		default:
			pr_err("rd: couldn't process '%s' for count", cmdv[1]);
			return -2;
		}
		args_consumed++;
	}

	get.byte_count = count;

	if (ioctl(cdevFd, RSMU_REG_READ, &get)) {
		pr_err("%s: failed", __func__);
		perror("RSMU_REG_READ failed");
		return -1;
	}

	if (1 == count) {
		printf("%04x: %02x\n", offset, get.bytes[0]);
	} else {
		aligned_offset = offset & 0xfff0;
		count_offset = offset & 0x000f;

		for (i = 0; i < count + count_offset; i++) {
			if (0 == (i % 16)) {
				printf("\n%04x: ", offset + i);
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

static int do_set_combo_mode(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_combomode set = {0};
	int args_consumed = 1;
	enum parser_result r;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll and mode argument", __func__);
		return -2;
	}

	r = get_ranged_uint(cmdv[0], (unsigned int *)&set.dpll, 0, 7, 0);
	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: dpll '%s' is not a valid uint", __func__, cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: dpll '%s' is out of range.", __func__, cmdv[0]);
		return -2;
	default:
		pr_err("%s: couldn't process dpll '%s'", __func__, cmdv[0]);
		return -2;
	}

	if (cmdc > 1 && !name_is_a_command(cmdv[1])) {
		r = get_ranged_uint(cmdv[1], (unsigned int *)&set.mode, 0, 3, 0);
		switch (r) {
		case PARSED_OK:
			break;
		case MALFORMED:
			pr_err("%s: mode '%s' is not a valid uint", __func__, cmdv[1]);
			return -2;
		case OUT_OF_RANGE:
			pr_err("%s: mode '%s' is out of range.", __func__, cmdv[1]);
			return -2;
		default:
			pr_err("%s: couldn't process '%s' for count", __func__, cmdv[1]);
			return -2;
		}
		args_consumed++;
	}

	if (ioctl(cdevFd, RSMU_SET_COMBOMODE, &set)) {
		pr_err("%s: failed - is DPLL %d valid for this part?", __func__, set.dpll);
		return -1;
	}

	printf("DPLL %d:  set combo mode to %d\n", set.dpll, set.mode);

	return args_consumed;
}


static int do_set_holdover_mode(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_holdover_mode set = {0};
	int args_consumed = 1;
	enum parser_result r;

	if (cmdc < 3 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required dpll, enable, and mode argument", __func__);
		return -2;
	}

	r = get_ranged_uint(cmdv[0], (unsigned int *)&set.dpll, 0, 7, 0);
	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: dpll '%s' is not a valid uint", __func__, cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: dpll '%s' is out of range.", __func__, cmdv[0]);
		return -2;
	default:
		pr_err("%s: couldn't process dpll '%s'", __func__, cmdv[0]);
		return -2;
	}

	if (cmdc > 1 && !name_is_a_command(cmdv[1])) {
		r = get_ranged_uint(cmdv[1], (unsigned int *)&set.enable, 0, 1, 0);
		switch (r) {
		case PARSED_OK:
			break;
		case MALFORMED:
			pr_err("%s: enable '%s' is not a valid uint", __func__, cmdv[1]);
			return -2;
		case OUT_OF_RANGE:
			pr_err("%s: enable '%s' is out of range.", __func__, cmdv[1]);
			return -2;
		default:
			pr_err("%s: couldn't process '%s' for count", __func__, cmdv[1]);
			return -2;
		}
		args_consumed++;
	}

	if (cmdc > 2 && !name_is_a_command(cmdv[2])) {
		r = get_ranged_uint(cmdv[2], (unsigned int *)&set.mode, 0, 1, 0);
		switch (r) {
		case PARSED_OK:
			break;
		case MALFORMED:
			pr_err("%s: mode '%s' is not a valid uint", __func__, cmdv[1]);
			return -2;
		case OUT_OF_RANGE:
			pr_err("%s: mode '%s' is out of range.", __func__, cmdv[1]);
			return -2;
		default:
			pr_err("%s: couldn't process '%s' for count", __func__, cmdv[1]);
			return -2;
		}
		args_consumed++;
	}

	if (ioctl(cdevFd, RSMU_SET_HOLDOVER_MODE, &set)) {
		pr_err("%s: failed - is DPLL %d valid for this part?", __func__, set.dpll);
		return -1;
	}

	return args_consumed;
}


static int do_set_output_tdc_go(int cdevFd, int cmdc, char *cmdv[])
{
	struct rsmu_set_output_tdc_go set = {0};
	int args_consumed = 1;
	enum parser_result r;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("%s: missing required tdc and enable arguments", __func__);
		return -2;
	}

	r = get_ranged_uint(cmdv[0], (unsigned int *)&set.tdc, 0, 3, 0);
	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("%s: tdc '%s' is not a valid uint", __func__, cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("%s: tdc '%s' is out of range.", __func__, cmdv[0]);
		return -2;
	default:
		pr_err("%s: couldn't process tdc '%s'", __func__, cmdv[0]);
		return -2;
	}

	if (cmdc > 1 && !name_is_a_command(cmdv[1])) {
		r = get_ranged_uint(cmdv[1], (unsigned int *)&set.enable, 0, 1, 0);
		switch (r) {
		case PARSED_OK:
			break;
		case MALFORMED:
			pr_err("%s: enable '%s' is not a valid uint", __func__, cmdv[1]);
			return -2;
		case OUT_OF_RANGE:
			pr_err("%s: enable '%s' is out of range.", __func__, cmdv[1]);
			return -2;
		default:
			pr_err("%s: couldn't process '%s' for count", __func__, cmdv[1]);
			return -2;
		}
		args_consumed++;
	}

	if (ioctl(cdevFd, RSMU_SET_OUTPUT_TDC_GO, &set)) {
		pr_err("%s: failed - is TDC %d valid for this part?", __func__, set.tdc);
		return -1;
	}

	return args_consumed;
}


static int do_wait(int fd, int cmdc, char *cmdv[])
{
	double time_arg;
	struct timespec ts;
	struct itimerval timer;
	enum parser_result r;

	if (cmdc < 1 || name_is_a_command(cmdv[0])) {
		pr_err("wait: requires sleep duration argument\n");
		return -2;
	}

	memset(&timer, 0, sizeof(timer));

	/* parse the double time offset argument */
	r = get_ranged_double(cmdv[0], &time_arg, 0.0, DBL_MAX);
	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("wait: '%s' is not a valid double", cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("wait: '%s' is out of range.", cmdv[0]);
		return -2;
	default:
		pr_err("wait: couldn't process '%s'", cmdv[0]);
		return -2;
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
	unsigned int count = 1;
	int args_consumed = 1;
	enum parser_result r;
	unsigned int val = 0;
	unsigned int offset;
	int i;

	if (cmdc < 2 || name_is_a_command(cmdv[0])) {
		pr_err("wr: missing required offset and byte_value argument");
		return -2;
	}

	/* Extend to access page registers and SYS RAM3 */
	r = get_ranged_uint(cmdv[0], &offset, 0, 0x20120000, 16);
	set.offset = offset;

	switch (r) {
	case PARSED_OK:
		break;
	case MALFORMED:
		pr_err("wr: offset '%s' is not a valid uint", cmdv[0]);
		return -2;
	case OUT_OF_RANGE:
		pr_err("wr: offset '%s' is out of range.", cmdv[0]);
		return -2;
	default:
		pr_err("wr: ofset couldn't process '%s'", cmdv[0]);
		return -2;
	}

	if (cmdc > 1 && !name_is_a_command(cmdv[1])) {
		r = get_ranged_uint(cmdv[1], &count, 1, 256, 0);
		switch (r) {
		case PARSED_OK:
			break;
		case MALFORMED:
			pr_err("wr: count '%s' is not a valid uint", cmdv[1]);
			return -2;
		case OUT_OF_RANGE:
			pr_err("wr: count '%s' is out of range.", cmdv[1]);
			return -2;
		default:
			pr_err("wr: couldn't process '%s' for count", cmdv[1]);
			return -2;
		}
		args_consumed++;
	}
	set.byte_count = count;

	if (cmdc < count + 2) {
		pr_err("wr: Not enough arguments to satisfy byte count %d", count);
		return -3;
	}

	for (i = 0; i < count; i++) {
		if (name_is_a_command(cmdv[2 + i])) {
			pr_err("wr: byte %d is a command, %s", i, cmdv[2 + i]);
		}

		r = get_ranged_uint(cmdv[2 + i], &val, 0, 256, 16);
		switch (r) {
		case PARSED_OK:
			break;
		case MALFORMED:
			pr_err("wr: val '%s' is not a valid uint", cmdv[2 + i]);
			return -2;
		case OUT_OF_RANGE:
			pr_err("wr: val '%s' is out of range.", cmdv[2 + i]);
			return -2;
		default:
			pr_err("wr: val couldn't process '%s'", cmdv[2 + i]);
			return -2;
		}
		args_consumed++;
		set.bytes[i] = val;
	}

	if (ioctl(cdevFd, RSMU_REG_WRITE, &set)) {
		pr_err("%s: failed", __func__);
		perror("RSMU_REG_WRITE failed");
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
	{ "get_ffo", &do_get_ffo },
	{ "get_state", &do_get_state },
	{ "rd", &do_rd },
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
	if( fd < 0 ) {
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
