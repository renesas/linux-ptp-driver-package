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

#
# Lock time_sync to 1Hz external signal
#
modprobe ptp_fc3 firmware=rsmufc3.bin
modprobe rsmu
rsmu_ctl /dev/rsmu0 time_sync_ext linuxptp/configs/fc3_wp.cfg 1

#
# Lock time_sync to 0.5Hz external signal
#
modprobe ptp_fc3 firmware=rsmufc3.timesync_0.5Hz.bin
modprobe rsmu firmware=rsmufc3.timesync_0.5Hz.bin
rsmu_ctl /dev/rsmu0 time_sync_ext linuxptp/configs/fc3_wp.cfg 2

./rsmu_ctl /dev/rsmu0 time_sync_ext fc3_wp.cfg 2
rsmu_ctl[14957.119]: Opening /dev/rsmu0 ...
rsmu_ctl[14960.198]: offset -432770200 s0 freq      -0
rsmu_ctl[14962.196]: offset -432770256 s1 freq     -28
rsmu_ctl[14967.764]: offset        -54 s2 freq     -55
rsmu_ctl[14971.763]: offset          0 s2 freq     -36
rsmu_ctl[14975.763]: offset         73 s2 freq      +0
rsmu_ctl[14979.764]: offset         25 s2 freq     -13
rsmu_ctl[14981.763]: offset       -100 s2 freq     -71
rsmu_ctl[14983.763]: offset          0 s2 freq     -36
rsmu_ctl[14987.765]: offset          0 s2 freq     -36
rsmu_ctl[14991.764]: offset         33 s2 freq     -20
rsmu_ctl[14995.763]: offset          0 s2 freq     -31
rsmu_ctl[14997.765]: offset          0 s2 freq     -31
rsmu_ctl[14999.765]: offset          0 s2 freq     -31
rsmu_ctl[15001.763]: offset          0 s2 freq     -31
rsmu_ctl[15003.764]: offset          0 s2 freq     -31
rsmu_ctl[15005.763]: offset          0 s2 freq     -31
rsmu_ctl[15007.763]: offset          0 s2 freq     -31
rsmu_ctl[15009.763]: offset          0 s2 freq     -31
rsmu_ctl[15011.765]: offset          0 s2 freq     -31
rsmu_ctl[15013.763]: offset          0 s2 freq     -31
rsmu_ctl[15015.763]: offset          0 s2 freq     -31
rsmu_ctl[15017.764]: offset          0 s2 freq     -31
rsmu_ctl[15019.765]: offset          0 s3 freq     -31
rsmu_ctl[15021.764]: offset          0 s3 freq     -31
rsmu_ctl[15023.763]: offset          0 s3 freq     -31


