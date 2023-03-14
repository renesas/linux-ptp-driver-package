BUILD
=====
# In rsmu_ctl directory
$ export CROSS_COMPILE=/usr/bin/aarch64-linux-gnu-   # Customize to build environment
$ make


Sample Build Log
================
$ make
DEPEND rsmu_ctl.c
DEPEND version.c
DEPEND util.c
DEPEND print.c
gcc -Wall -DVER=0.1      -c -o rsmu_ctl.o rsmu_ctl.c
gcc -Wall -DVER=0.1      -c -o print.o print.c
gcc -Wall -DVER=0.1      -c -o util.o util.c
gcc -Wall -DVER=0.1      -c -o version.o version.c
gcc   rsmu_ctl.o print.o util.o version.o  -lm -lrt -pthread  -o rsmu_ctl


Help
====
usage: rsmu_ctl [options] <device> -- [command]

 device         MISC device, ex. /dev/rsmu0
 options
 -l [num]       set the logging level to 'num'
 -q             do not print messages to the syslog
 -Q             do not print messages to stdout
 -v             prints the software version and exits
 -h             prints this message and exits

 commands
 specify commands with arguments. Can specify multiple
 commands to be executed in order.
  get_ffo   <dpll_n>                              get <dpll_n> FFO in ppb
  get_state <dpll_n>                              get state of <dpll_n>
  rd        <offset (hex)> [count]                read [count] bytes from offset (default count 1)
  set_combo_mode  <dpll_n> <mode>                 set combo mode
                                                    CM
                                                      0   Hold Disabled
                                                      1-3 Hold Enabled
                                                    Sabre
                                                      0   PHASE_FREQ_OFFSET
                                                      1   FREQ_OFFSET
                                                      2   FAST_AVG_FREQ_OFFSET
                                                      3   PLL_COMBO_MODE_HOLD
  set_holdover_mode <dpll_n> <enable> <mode>      set holdover mode
  set_output_tdc_go <tdc_n> <enable>              set output TDC go bit
  wait      <seconds>                             pause <seconds> between commands
  wr        <offset (hex)> <count> <val (hex)>    write <val> to <offset>



Sample Output
=============
#
# DPLL0 is locked to 1 PPS
#
$ ./rsmu_ctl /dev/rsmu1 get_state 0
rsmu_ctl[91268.274]: Opening /dev/rsmu1 ...
DPLL 0:  state = 4		// E_SRVLOTIMELOCKEDSTATE = 4

$ ./rsmu_ctl /dev/rsmu1 get_ffo 0
rsmu_ctl[91300.202]: Opening /dev/rsmu1 ...
DPLL 0:  ffo =  401.010145 ppb

#
# Get DPLL state for DPLL 2
#
$ ./rsmu_ctl /dev/rsmu1 get_state 2
rsmu_ctl[89620.878]: Opening /dev/rsmu1 ...
DPLL 2:  state = 9   		// E_SRVLOSTATEINVALID = 9, can be open loop or disabled

#
# Read DPLL0_STATUS-DPLL7_STATUS, SYS_STATUS, APLL_STATUS
#
$ ./rsmu_ctl /dev/rsmu1 rd 2010c054 10
rsmu_ctl[89910.275]: Opening /dev/rsmu1 ...

2010c054: -- -- -- -- 13 00 05 00 00 00 00 00 13 00

#
# Read 80 bytes
#
$ ./rsmu_ctl /dev/rsmu1 rd 2010c000 80
rsmu_ctl[89260.382]: Opening /dev/rsmu1 ...

2010c000: 74 25 f4 bd 6e 7f 3d e1 91 62 4d be 00 00 00 00
2010c010: 23 4c 00 09 a0 00 00 00 00 00 00 00 00 00 00 05
2010c020: 37 01 02 00 09 08 07 00 d5 b6 00 00 00 00 00 00
2010c030: 35 06 01 40 47 00 0f 01 01 00 00 00 08 08 01 00
2010c040: 5b 01 00 5b 55 00 50 00 00 00 00 00 00 00 00 00


