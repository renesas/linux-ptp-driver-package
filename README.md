# linux-ptp-driver-package

The PTP and MFD files are available for customer use and are in the process of being submitted to the Linux kernel.

The MISC files are not part of the Linux kernel distribution.

For each pcm4l release, there is a recommended driver version. All driver versions can be used with ptp4l stand-alone.


## README.md
This README.md is a Markdown document that can be viewed by any text editor.

Github natively shows the rendered Markdown document.

Microsoft Studio Code (https://code.visualstudio.com/) can be used to view a rendered Markdown document. Open the file in the editor and press Ctrl+Shift+V.

## Revision History
| Version               | Package                                                 |
|-----------------------|---------------------------------------------------------|
| pcm4l 4.3.0           | 2.0, 2.1                                                |
| pcm4l 4.2.1++         | 2.0, 2.1                                                |
| pcm4l 4.2.0           | Renesas_MFD_PTP_MISC_patches_2022_07_25_5ecd2144.tar.gz |
| pcm4l 4.1.4           | Renesas_MFD_PTP_MISC_patches_2022_02_24_e9d9162c.tar.gz |
| pcm4l 4.1.3 and below	| Renesas_MFD_PTP_MISC_patches_2021_09_17_59227acc.tar.gz |


# Table of Contents
1. [Repository Structure](#repository-structure)
2. [Dependencies](#dependencies)
    * [Linux Kernel](#linux-kernel)
    * [linuxptp](#linuxptp)
3. [Getting Started](#getting-started)
   * [Integrate driver files](#integrate-driver-files)
   * [Apply patch files](#apply-patch-files-to-driver-files-andor-kernel)
     * [Linux v6.3 - v6.5-rc4](#linux-v63---v65-rc4) 
     * [Linux v6.1 - v6.2](#linux-v61---v62) 
     * [Linux v5.8 - v6.0](#linux-v518---v60)
     * [Linux v5.4 - v5.7](#linux-v54---v57)
     * [Linux v4.13 - v5.3](#linux-v413---v53)
     * [Linux v4.11 - v4.12](#linux-v411---412)
     * [Linux v4.10 and below](#linux-v410-and-below)
   * [Configure Kernel](#configure-kernel)
   * [Update device tree in Kernel](#update-device-tree)
   * [Test Compile Drivers](#test-compile-drivers)
   * [Sample Session Output](#sample-session-output)
5. [Example Loading of Drivers on Xilinx ZCU670 Board](#example-loading-of-drivers-on-xilinx-zcu670-board)
6. [Next Steps](#next-steps)

# Repository Structure
This repository contains the Renesas PTP driver files to be used with the PTP Hardware Clock (PHC) API infrastructure in Linux.

* linux  
  This directory contains driver files in the same location as the linux kernel directory structure.

  * archive
    * This directory contains deprecated driver packages  

  * Documentation/devicetree/bindings/ptp
    * The “Open Firmware Device Tree” is used to describe the system hardware in Linux. This directory contains device tree binding example for the device.

  * drivers/mfd
    * This directory contains rsmu_core driver that controls the I2C or SPI access to the device.

  * drivers/ptp
      * This directory contains the PTP driver that implements the PHC API functionality for the device.

  * drivers/misc
    * This directory contains the misc driver that implements device functionality outside of the PHC API that is used by Renesas PTP Clock Management for Linux (pcm4l) software.

```
├── archive
├── LICENSE
├── linux
│   ├── Documentation
│   │   └── devicetree
│   │       └── bindings
│   │           └── ptp
│   │               ├── ptp-idt82p33.yaml
│   │               └── ptp-idtcm.yaml
│   ├── drivers
│   │   ├── mfd
│   │   │   ├── Kconfig
│   │   │   ├── Makefile
│   │   │   ├── rsmu_core.c
│   │   │   ├── rsmu.h
│   │   │   ├── rsmu_i2c.c
│   │   │   └── rsmu_spi.c
│   │   ├── misc
│   │   │   ├── Kconfig
│   │   │   ├── Makefile
│   │   │   ├── rsmu_cdev.c
│   │   │   ├── rsmu_cdev.h
│   │   │   ├── rsmu_cm.c
│   │   │   └── rsmu_sabre.c
│   │   └── ptp
│   │       ├── Kconfig
│   │       ├── Makefile
│   │       ├── ptp_clockmatrix.c
│   │       ├── ptp_clockmatrix.h
│   │       ├── ptp_idt82p33.c
│   │       └── ptp_idt82p33.h
│   └── include
│       ├── linux
│       │   └── mfd
│       │       ├── idt82p33_reg.h
│       │       ├── idt8a340_reg.h
│       │       └── rsmu.h
│       └── uapi
│           └── linux
│               └── rsmu.h
├── ManualInstall.md
├── patches
├── README.md
└── scripts
```
# Dependencies

## Linux Kernel

https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/

* PTP Hardware Clock (PHC) API
    * CONFIG_PTP_1588_CLOCK=y  
      https://www.kernel.org/doc/html/latest/driver-api/ptp.html
    
    * v4.13 - ptp_schedule_worker
      * Introduced to PHC API in Linux Kernel v4.13  
        https://patchwork.kernel.org/project/linux-omap/patch/20170726221138.12986-2-grygorii.strashko@ti.com/<br><br>

    * v5.4 - PTP_STRICT_FLAGS
      * Introduced to PHC API in Linux Kernel v5.4 as part of the "[PATCH net 08/13] ptp: Introduce strict checking of external time stamp options." as part of a larger "[PATCH net 00/13] ptp: Validate the ancillary ioctl flags more carefully."
      
        https://github.com/torvalds/linux/commit/e2a689ab8f7a01815e3ebb4257b42587f752257f

        https://www.spinics.net/lists/netdev/msg613039.html  
        [PATCH net 08/13] ptp: Introduce strict checking of external time stamp options.

        https://www.spinics.net/lists/netdev/msg613032.html  
        [PATCH net 00/13] ptp: Validate the ancillary ioctl flags more carefully.

    * v5.8 Adjust Phase support
      * Introduced to PHC API in Linux Kernel v5.8
        https://git.kernel.org/pub/scm/linux/kernel/git/stable/linux.git/tree/drivers?h=linux-5.8.y
        
        [v2,net-next,1/3] ptp: Add adjphase function to support phase offset control  
        https://patchwork.kernel.org/project/linux-kselftest/patch/1588390538-24589-2-git-send-email-vincent.cheng.xh@renesas.com/

        [v2,net-next,2/3] ptp: Add adjust_phase to ptp_clock_caps capability.  
        https://patchwork.kernel.org/project/linux-kselftest/patch/1588390538-24589-3-git-send-email-vincent.cheng.xh@renesas.com/  

        [PATCH net-next V2] Let the ADJ_OFFSET interface respect the ADJ_NANO flag for PHC devices.  
        https://www.spinics.net/lists/netdev/msg655706.html

* Open Firmware
    * CONFIG_OF=y
## linuxptp

* ptp4l
  * Adjust Phase support
    * added to ptp4l in v3.0  
      https://sourceforge.net/p/linuxptp/code/ci/v3.0/tree/  

# Getting Started

  1. [Integrate driver files](#integrate-driver-files)  
     * [Option 1 - Run install script](#option-1---run-install-script)
     * [Option 2 - Manual Integration](#option-2---manual-integration)
  2. [Apply patch files](#apply-patch-files-to-driver-files-andor-kernel)
     * [Linux v5.8+](#linux-v518)
     * [Linux v5.4 - v5.7](#linux-v54---v57)
     * [Linux v4.13 - v5.3](#linux-v413---v53)
     * [Linux v4.11 - v4.12](#linux-v411---412)
     * [Linux v4.10 and below](#linux-v410-and-below)
  3. [Configure Kernel](#configure-kernel)
  4. [Update device tree in Kernel](#update-device-tree)
  5. [Test Compile Drivers](#test-compile-drivers)
  6. [Sample Session Output](#sample-session-output)

## Integrate driver files
There are two options below, choose the one that works best for you.
### Option 1 -  Run install script
*installDriver.sh* in the *scripts* directory automates the procedure outlined in [Option 2](#option-2-manual-integration).  The script was developed and tested with sed (GNU sed 4.2.2).

```
#
# Run installDriver.sh to copy/integrate driver files to target kernel source
#

In ptp-driver-package's root dir:

$ scripts/installDriver.sh <source dir> <target dir>

  <source dir> = location of ptp-driver-package
  <target dir> = location of linux kernel
```

For example:
```
media
├── ptp-driver-package
│   ├── linux
│   ├── patches
│   └── scripts
└── linux-stable

In ptp-driver-package root directory:

$ scripts/installDriver.sh . ../linux-stable
```

### Option 2 - Manual Integration
[Manual Integration](./ManualInstall.md)

## Apply patch files to driver files and/or kernel
The driver files were primarily developed and tested on v5.4 kernel.

The driver files can be ported to various linux kernels with minor modifications.  

If a specific kernel feature is needed, the driver may be able to be modified to workaround the missing feature with no loss of functionality.  

Below are some example scenarios where patch files can be applied to modify the driver or the kernel source.

### Linux v6.3 - v6.5-rc4

In 6.3, i2c_device_id parameter is removed.

```
In ptp-driver-package repository root directory, target linux kernel is "../linux-stable":

$ patch --directory=../linux-stable -p1 < patches/0001-Change-spi-remove-func-return-void.patch
$ patch --directory=../linux-stable -p1 < patches/0001-i2c-Make-remove-callback-return-void.patch
$ patch --directory=../linux-stable -p1 < patches/0002-mfd-rsmu_i2c-Convert-to-i2c-s-.probe_new.patch
$ patch --directory=../linux-stable -p1 < patches/0003-mfd-Switch-i2c-drivers-back-to-use-.probe.patch
```

### Linux v6.1 - v6.2

In 6.1, I2C driver changed to return void from int.

```
In ptp-driver-package repository root directory, target linux kernel is "../linux-stable":

$ patch --directory=../linux-stable -p1 < patches/0001-Change-spi-remove-func-return-void.patch
$ patch --directory=../linux-stable -p1 < patches/0001-i2c-Make-remove-callback-return-void.patch
```

### Linux v5.18 - v6.0

In 5.18, SPI driver "remove" callback was changed to a void function.

```
In ptp-driver-package repository root directory, target linux kernel is "../linux-stable":

$ patch --directory=../linux-stable -p1 < patches/0001-Change-spi-remove-func-return-void.patch
```

### Linux v5.8 - v5.17

No modifications needed.  Driver files should work as-is.

### Linux v5.4 - v5.7

Need to apply write phase adjust patch to kernel.

```
In ptp-driver-package repository root directory, target linux kernel is "../linux-stable":

$ patch --directory=../linux-stable -p1 < patches/0001-Introduce-adjphase-to-PHC-API.patch
```
### Linux v4.13 - v5.3
Need to apply introduce write phase adjust and remove PTP_STRICT_FLAGS from PTP driver patch.
```
In ptp-driver-package repository root directory, target linux kernel is "../linux-stable":

$ patch --directory=../linux-stable -p1 < patches/0001-Introduce-adjphase-to-PHC-API.patch

$ patch --directory=../linux-stable -p1 < patches/0001-Remove-PTP_STRICT_FLAGS-usage.patch
```
### Linux v4.11 - 4.12
Need to apply introduce ptp_schedule_worker, introduce write phase adjust and remove PTP_STRICT_FLAGS from PTP driver patch.
```
In ptp-driver-package repository root directory, target linux kernel is "../linux-stable":

$ patch --directory=../linux-stable -p1 < patches/0001-ptp-introduce-ptp-auxiliary-worker.patch

$ patch --directory=../linux-stable -p1 < patches/0001-Introduce-adjphase-to-PHC-API.patch

$ patch --directory=../linux-stable -p1 < patches/0001-Remove-PTP_STRICT_FLAGS-usage.patch
```

### Linux v4.10 and below
Out of scope for this document.

Manually inspect and backport missing dependencies as required.

## Configure Kernel

```
In linux kernel source root directory:

$ make menuconfig
```

Select the following, adjust I2C or SPI and devices as required.

Make sure to **"Save"** before moving to subsequent configuration sections. 

Some configuration options may not show up if prior selections are not selected and saved.

```
Device Drivers  --->

    [*] Device Tree and Open Firmware support  ----
    <Save>
 
    Multifunction device drivers  --->
        <M> Renesas Synchronization Management Unit with I2C (NEW)
        < > Renesas Synchronization Management Unit with SPI (NEW)
    <Save>
 
    {*} PTP clock support  --->
        <M> IDT CLOCKMATRIX as PTP clock (NEW)
        < > IDT 82P33xxx PTP clock (NEW)
    <Save>
 
    Misc devices  --->
        <M> Renesas Synchronization Management Unit (SMU) (NEW)
    <Save>
```

## Verify .config
I2C/SPI is dependent on device access selection.
IDTCM/IDT82P33 is dependent on device selection.

```
...
CONFIG_OF=y
...
#
# PTP clock support
#
CONFIG_PTP_1588_CLOCK=y
CONFIG_PTP_1588_CLOCK_IDTCM=m
CONFIG_PTP_1588_CLOCK_IDT82P33=m
...
#
# Misc devices
#
CONFIG_RSMU=m
...
#
# Multifunction device drivers
#
CONFIG_MFD_CORE=m
CONFIG_MFD_RSMU_I2C=m
CONFIG_MFD_RSMU_SPI=m
```
## Update device tree

### Sample I2C Entry ClockMatrix
```

	phc@5b {
		compatible = "idt,8a34000";
		reg = <0x5b>;
	};
```

### Sample SPI Entry ClockMatrix
```
	phc@0 { 
		compatible = "idt,8a34000";
		spi-max-frequency = <10000000>;
		reg = <0>;
	};
```

### Sample I2C Entry 82P33x
```
	phc@51 { 
		compatible = "idt,82p33810";
		reg = <0x51>;
	};
```
## Test Compile Drivers
Rebuilding kernel and installation to the board is outside the scope of this document.

Below shows a quick test compile to confirm configuration is correct and the driver can be compiled with the kernel source.

```
# Compile drivers/ptp directory
make -j $(nproc) drivers/ptp/
 
# Compile drivers/mfd directory
make -j $(nproc) drivers/mfd/
 
# Compile drivers/misc directory
make -j $(nproc) drivers/misc/
```

### Verify Drivers Compiled

If the drivers compiled properly should see a subset of object files that is dependent on configuration of I2C/SPI and Renesas PTP devices.

```
ls -1 drivers/mfd/rsmu*.o
drivers/mfd/rsmu_core.o
drivers/mfd/rsmu_i2c.o
drivers/mfd/rsmu-i2c.o
drivers/mfd/rsmu_spi.o
drivers/mfd/rsmu-spi.o
 
ls -1 drivers/misc/rsmu*.o
drivers/misc/rsmu_cdev.o
drivers/misc/rsmu_cm.o
drivers/misc/rsmu.o
drivers/misc/rsmu_sabre.o
 
ls -1 drivers/ptp/*.o
drivers/ptp/ptp_chardev.o          
drivers/ptp/ptp_clockmatrix.o  
drivers/ptp/ptp_clock.o
drivers/ptp/ptp_idt82p33.o
drivers/ptp/ptp.o
```
# Sample Session Output

```
# media
# ├── ptp-driver-package
# │   ├── linux
# │   ├── patches
# │   └── scripts
# └── linux-stable (v6.0)
# 
# In ptp-driver-package root directory:
$ scripts/installDriver.sh . ../linux-stable
SRC_DIR = .
DST_DIR = ../linux-stable

Remove deprecated files
=======================
remove_deprecated_files:
    TGT_DIR = ../linux-stable
rm -f ../linux-stable/drivers/mfd/rsmu_private.h
rm -f ../linux-stable/drivers/ptp/idt8a340_reg.h
rm -f ../linux-stable/drivers/ptp/ptp_idt_debugfs.h
rm -f ../linux-stable/drivers/ptp/8a3xxxx_reg.h

Documentation/devicetree/bindings/ptp
=====================================
cp ./linux/Documentation/devicetree/bindings/ptp/ptp-idt82p33.yaml ../linux-stable/Documentation/devicetree/bindings/ptp/ptp-idt82p33.yaml
cp ./linux/Documentation/devicetree/bindings/ptp/ptp-idtcm.yaml ../linux-stable/Documentation/devicetree/bindings/ptp/ptp-idtcm.yaml

MFD
===
clean_driver_mfd_Makefile:
    TGT_FILE = ../linux-stable/drivers/mfd/Makefile
insert_driver_mfd_Makefile:
    SRC_FILE = ./linux/drivers/mfd/Makefile
    TGT_FILE = ../linux-stable/drivers/mfd/Makefile
clean_driver_mfd_Kconfig:
    TGT_FILE = ../linux-stable/drivers/mfd/Kconfig
insert_driver_mfd_Kconfig:
    SRC_FILE = ./linux/drivers/mfd/Kconfig
    TGT_FILE = ../linux-stable/drivers/mfd/Kconfig
cp ./linux/drivers/mfd/rsmu.h ../linux-stable/drivers/mfd/rsmu.h
cp ./linux/drivers/mfd/rsmu_core.c ../linux-stable/drivers/mfd/rsmu_core.c
cp ./linux/drivers/mfd/rsmu_i2c.c ../linux-stable/drivers/mfd/rsmu_i2c.c
cp ./linux/drivers/mfd/rsmu_spi.c ../linux-stable/drivers/mfd/rsmu_spi.c

cp ./linux/include/linux/mfd/idt82p33_reg.h ../linux-stable/include/linux/mfd/idt82p33_reg.h
cp ./linux/include/linux/mfd/idt8a340_reg.h ../linux-stable/include/linux/mfd/idt8a340_reg.h
cp ./linux/include/linux/mfd/rsmu.h ../linux-stable/include/linux/mfd/rsmu.h

PTP
===
clean_driver_ptp_Kconfig:
    TGT_FILE = ../linux-stable/drivers/ptp/Kconfig
insert_driver_ptp_Kconfig:
    SRC_FILE = ./linux/drivers/ptp/Kconfig
    TGT_FILE = ../linux-stable/drivers/ptp/Kconfig
clean_driver_ptp_Makefile:
    TGT_FILE = ../linux-stable/drivers/ptp/Makefile
insert_driver_ptp_Makefile:
    SRC_FILE = ./linux/drivers/ptp/Makefile
    TGT_FILE = ../linux-stable/drivers/ptp/Makefile
cp ./linux/drivers/ptp/ptp_clockmatrix.c ../linux-stable/drivers/ptp/ptp_clockmatrix.c
cp ./linux/drivers/ptp/ptp_clockmatrix.h ../linux-stable/drivers/ptp/ptp_clockmatrix.h
cp ./linux/drivers/ptp/ptp_idt82p33.c ../linux-stable/drivers/ptp/ptp_idt82p33.c
cp ./linux/drivers/ptp/ptp_idt82p33.h ../linux-stable/drivers/ptp/ptp_idt82p33.h

MISC
====
clean_driver_misc_Makefile:
    TGT_FILE = ../linux-stable/drivers/misc/Makefile
insert_driver_misc_Makefile:
    SRC_FILE = ./linux/drivers/misc/Makefile
    TGT_FILE = ../linux-stable/drivers/misc/Makefile
clean_driver_misc_Kconfig:
    TGT_FILE = ../linux-stable/drivers/misc/Kconfig
insert_driver_misc_Kconfig:
    SRC_FILE = ./linux/drivers/misc/Kconfig
    TGT_FILE = ../linux-stable/drivers/misc/Kconfig
cp ./linux/drivers/misc/rsmu_cdev.c ../linux-stable/drivers/misc/rsmu_cdev.c
cp ./linux/drivers/misc/rsmu_cdev.h ../linux-stable/drivers/misc/rsmu_cdev.h
cp ./linux/drivers/misc/rsmu_cm.c ../linux-stable/drivers/misc/rsmu_cm.c
cp ./linux/drivers/misc/rsmu_sabre.c ../linux-stable/drivers/misc/rsmu_sabre.c

cp ./linux/include/uapi/linux/rsmu.h ../linux-stable/include/uapi/linux/rsmu.h

$ patch --directory=../linux-stable -p1 < patches/0001-Change-spi-remove-func-return-void.patch
patching file drivers/mfd/rsmu_spi.c

# In linux-stable
$ cd ../linux-stable
$ make menuconfig
  HOSTCC  scripts/basic/fixdep
  UPD     scripts/kconfig/mconf-cfg
  ...
*** End of the configuration.
*** Execute 'make' to start the build or try 'make help'.

$ make -j $(nproc) drivers/ptp/
Makefile:674: include/config/auto.conf: No such file or directory
Makefile:711: include/config/auto.conf.cmd: No such file or directory
  SYNC    include/config/auto.conf.cmd
  HOSTCC  scripts/kconfig/conf.o
  HOSTLD  scripts/kconfig/conf
  ...
  CC      drivers/ptp/ptp_clock.o
  CC      drivers/ptp/ptp_chardev.o
  CC      drivers/ptp/ptp_vclock.o
  CC      drivers/ptp/ptp_sysfs.o
  CC [M]  drivers/ptp/ptp_clockmatrix.o
  CC [M]  drivers/ptp/ptp_idt82p33.o
  CC [M]  drivers/ptp/ptp_kvm_x86.o
  CC [M]  drivers/ptp/ptp_kvm_common.o
  AR      drivers/ptp/built-in.a
  LD [M]  drivers/ptp/ptp_kvm.o

$ make -j $(nproc) drivers/mfd/
  DESCEND objtool
  CALL    scripts/atomic/check-atomics.sh
  CALL    scripts/checksyscalls.sh
  ...
  CC [M]  drivers/mfd/rsmu_core.o
  CC [M]  drivers/mfd/rsmu_i2c.o
  CC [M]  drivers/mfd/rsmu_spi.o
  ...

$ make -j $(nproc) drivers/misc/
  DESCEND objtool
  CALL    scripts/atomic/check-atomics.sh
  CALL    scripts/checksyscalls.sh
  ...
  CC [M]  drivers/misc/rsmu_cdev.o
  CC [M]  drivers/misc/rsmu_cm.o
  CC [M]  drivers/misc/rsmu_sabre.o
  AR      drivers/misc/built-in.a
  LD [M]  drivers/misc/rsmu.o
```

# Example Loading of Drivers on Xilinx ZCU670 Board
```
#
# Before loading drivers
#
root@xilinx-zcu670-2021_2:~# lsmod
    Tainted: G
macb 53248 0 - Live 0xffff800008dcb000
phylink 28672 1 macb, Live 0xffff800008dbd000
uio_pdrv_genirq 16384 0 - Live 0xffff800008db0000

#
# Load MFD/MISC driver
#
#   MFD also supports loading configuration file in a system configuration that
#   does not have a corresponding PHC driver.
#
#   By default the MFD driver will look for 'rsmu8A34xxx.bin', if not found an error message
#   will display, but this can be ignored if configuration is taken care of by PHC driver.
#
root@xilinx-zcu670-2021_2:~# modprobe rsmu-i2c
root@xilinx-zcu670-2021_2:~# [  428.620347] rsmu-cdev 8a3400x-cdev.2.auto: requesting firmware 'rsmu8A34xxx.bin'
root@xilinx-zcu670-2021_2:~# [  428.627829] rsmu-cdev 8a3400x-cdev.2.auto: Direct firmware load for rsmu8A34xxx.bin failed with error -2
root@xilinx-zcu670-2021_2:~# [  428.637368] rsmu-cdev 8a3400x-cdev.2.auto: Loading firmware rsmu8A34xxx.bin failed !!!
root@xilinx-zcu670-2021_2:~# [  428.631925] rsmu-cdev 8a3400x-cdev.2.auto: Probe rsmu0 successful

#
# After loading MFD/MISC driver
#
root@xilinx-zcu670-2021_2:~# lsmod
    Tainted: G
rsmu 20480 0 - Live 0xffff800008dd9000 (O)
rsmu_i2c 16384 0 - Live 0xffff800008db5000 (O)
macb 53248 0 - Live 0xffff800008dcb000
phylink 28672 1 macb, Live 0xffff800008dbd000
uio_pdrv_genirq 16384 0 - Live 0xffff800008db0000

#
# Load PTP driver
#
root@xilinx-zcu670-2021_2:~# modprobe ptp_clockmatrix firmware=idtcm.bin.zcu670
[  486.598698] 8a3400x-phc 8a3400x-phc.1.auto: 4.8.8, Id: 0x4001  HW Rev: 5  OTP Config Select: 15
[  486.607406] 8a3400x-phc 8a3400x-phc.1.auto: requesting firmware 'idtcm.bin.zcu670'
[  490.404990] 8a3400x-phc 8a3400x-phc.1.auto: PLL1 registered as ptp1

#
# After loading PTP driver
#
root@xilinx-zcu670-2021_2:~# lsmod
    Tainted: G
ptp_clockmatrix 32768 0 - Live 0xffff800008ddf000 (O)
rsmu 20480 0 - Live 0xffff800008dd9000 (O)
rsmu_i2c 16384 0 - Live 0xffff800008db5000 (O)
macb 53248 0 - Live 0xffff800008dcb000
phylink 28672 1 macb, Live 0xffff800008dbd000
uio_pdrv_genirq 16384 0 - Live 0xffff800008db0000

# Check PTP clock name for ClockMatrix
root@xilinx-zcu670-2021_2:~# cat /sys/class/ptp/ptp1/clock_name
IDT CM TOD1
```
# Next Steps

Refer to Section 3.1 Sanity Testing of "Linux PTP Using PHC Adjust Phase Quick Start Manual" for additional board bring-up steps.
## Linux PTP Using PHC Adjust Phase Quick Start Manual
https://www.renesas.com/us/en/document/mas/linux-ptp-using-phc-adjust-phase-quick-start-manual?language=en




