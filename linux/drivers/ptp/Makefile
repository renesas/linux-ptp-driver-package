#
# Makefile for PTP 1588 clock support.
#

ptp-y					:= ptp_clock.o ptp_chardev.o ptp_sysfs.o
obj-$(CONFIG_PTP_1588_CLOCK)		+= ptp.o
obj-$(CONFIG_PTP_1588_CLOCK_IDTCM)	+= ptp_clockmatrix.o
obj-$(CONFIG_PTP_1588_CLOCK_IDT82P33)	+= ptp_idt82p33.o
obj-$(CONFIG_PTP_1588_CLOCK_FC3W)	+= ptp_fc3.o

