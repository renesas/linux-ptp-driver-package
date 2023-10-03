#
# Copyright (C) 2021 Vincent Cheng <vincent.cheng.xh@renesas.com>
#
# This was derived from phc_ctl by Richard Cochran <richardcochran@gmail.com>
# 
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

KBUILD_OUTPUT =

DEBUG	=
CC	= $(CROSS_COMPILE)gcc
VER     = -DVER=$(version)
CFLAGS	= -Wall $(VER) $(incdefs) $(DEBUG) $(EXTRA_CFLAGS) -I linuxptp
LDLIBS	= -lm -lrt -pthread $(EXTRA_LDFLAGS)
PRG	= rsmu_ctl

OBJ	= $(addprefix linuxptp/, config.o clockadj.o hash.o interface.o msg.o phc.o pmc_agent.o \
 pmc_common.o sk.o tlv.o transport.o raw.o print.o util.o\
 udp.o udp6.o uds.o linreg.o ntpshm.o nullf.o pi.o refclock_sock.o servo.o)

OBJECTS	= $(OBJ) rsmu_ctl.o version.o
SRC	= $(OBJECTS:.o=.c)
DEPEND	= $(OBJECTS:.o=.d)
srcdir	:= $(dir $(lastword $(MAKEFILE_LIST)))
incdefs := $(shell $(srcdir)/incdefs.sh)
version := $(shell $(srcdir)/version.sh $(srcdir))
VPATH	= $(srcdir)

all: $(PRG)
rsmu_ctl: $(OBJECTS)

version.o: .version version.sh $(filter-out version.d,$(DEPEND))

.version: force
	@echo $(version) > .version.new; \
	cmp -s .version .version.new || cp .version.new .version; \
	rm -f .version.new;

force:

clean:
	rm -f $(OBJECTS) $(DEPEND) $(PRG)

distclean: clean
	rm -f .version

# Implicit rule to generate a C source file's dependencies.
%.d: %.c
	@echo DEPEND $<; \
	rm -f $@; \
	$(CC) -MM $(CPPFLAGS) $(CFLAGS) $< > $@.$$$$; \
	sed 's,\($*\)\.o[ :]*,\1.o $@ : ,g' < $@.$$$$ > $@; \
	rm -f $@.$$$$

ifneq ($(MAKECMDGOALS), clean)
ifneq ($(MAKECMDGOALS), distclean)
-include $(DEPEND)
endif
endif

.PHONY: all force clean distclean
