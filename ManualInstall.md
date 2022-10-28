# Manual Integration
1. [Remove Deprecated Files](#remove-deprecated-files)
2. [Remove Current Driver Fles](#remove-current-driver-files)
3. [Integrate Kconfig and Makefile](#integrate-kconfig-and-makefile)
   * [MFD](#mfd)
     * [Kconfig](#kconfig)
     * [Makefile](#makefile)
   * [PTP](#ptp)
     * [Kconfig](#kconfig-1)
     * [Makefile](#makefile-1)
   * [MISC](#misc)
     * [Kconfig](#kconfig-2)
     * [Makefile](#makefile-2)

4. [Copy driver files](#copy-driver-files)


# Remove Deprecated Files
```
# Remove Deprecated Renesas PTP driver files (relative to root of linux kernel)
rm -f drivers/mfd/rsmu_private.h
rm -f drivers/ptp/idt8a340_reg.h
rm -f drivers/ptp/ptp_idt_debugfs.h
rm -f drivers/ptp/8a3xxxx_reg.h
```
# Remove Current Driver Files
```
## Remove Renesas PTP driver files (relative to root of linux kernel)
rm -f Documentation/devicetree/bindings/ptp/ptp-idt82p33.yaml
rm -f Documentation/devicetree/bindings/ptp/ptp-idtcm.yaml

rm -f drivers/mfd/rsmu.h
rm -f drivers/mfd/rsmu_core.c
rm -f drivers/mfd/rsmu_i2c.c
rm -f drivers/mfd/rsmu_spi.c
rm -f include/linux/mfd/idt82p33_reg.h
rm -f include/linux/mfd/idt8a340_reg.h
rm -f include/linux/mfd/rsmu.h

rm -f drivers/ptp/ptp_clockmatrix.c
rm -f drivers/ptp/ptp_clockmatrix.h
rm -f drivers/ptp/ptp_idt82p33.c
rm -f drivers/ptp/ptp_idt82p33.h

rm -f drivers/misc/rsmu_cdev.*
rm -f drivers/misc/rsmu_cm.c
rm -f drivers/misc/rsmu_sabre.c
rm -f include/uapi/linux/rsmu.h
```
# Integrate Kconfig and Makefile
Kconfig and Makefile share content with other drivers so these files must be integrated manually.  Cut and paste the corresponding Renesas sections into the target Kconfig and Makefile accordingly.

Snippets below show how the Renesas specific sections are integrated into Linux Kernel v6.0 Kconfig and Makefile.

## MFD
### Kconfig
```
drivers/mfd/Kconfig
 ...
 config MFD_INTEL_M10_BMC
    tristate "Intel MAX 10 Board Management Controller"
    depends on SPI_MASTER 
    select REGMAP_SPI_AVMM
    select MFD_CORE
    ...

+config MFD_RSMU_I2C
+	tristate "Renesas Synchronization Management Unit with I2C"
+	depends on I2C && OF
+	select MFD_CORE
+	select REGMAP_I2C
+	help
+	  Support for the Renesas Synchronization Management Unit, such as
+	  Clockmatrix and 82P33XXX series. This option supports I2C as
+	  the control interface.
+
+	  This driver provides common support for accessing the device.
+	  Additional drivers must be enabled in order to use the functionality
+	  of the device.
+
+config MFD_RSMU_SPI
+	tristate "Renesas Synchronization Management Unit with SPI"
+	depends on SPI && OF
+	select MFD_CORE
+	select REGMAP_SPI
+	help
+	  Support for the Renesas Synchronization Management Unit, such as
+	  Clockmatrix and 82P33XXX series. This option supports SPI as
+	  the control interface.
+
+	  This driver provides common support for accessing the device.
+	  Additional drivers must be enabled in order to use the functionality
+	  of the device.
+
 endmenu
 endif
```
### Makefile
```
drivers/mfd/Makefile
 ...
 obj-$(CONFIG_MFD_ATC260X)         += atc260x-core.o
 obj-$(CONFIG_MFD_ATC260X_I2C)     += atc260x-i2c.o
 
+rsmu-i2c-objs                := rsmu_core.o rsmu_i2c.o
+rsmu-spi-objs                := rsmu_core.o rsmu_spi.o
+obj-$(CONFIG_MFD_RSMU_I2C)   += rsmu-i2c.o
+obj-$(CONFIG_MFD_RSMU_SPI)   += rsmu-spi.o
```
## PTP
### Kconfig
```
drivers/ptp/Kconfig
 ...
+config PTP_1588_CLOCK_IDTCM
+       tristate "IDT CLOCKMATRIX as PTP clock"
+       select PTP_1588_CLOCK
+       default n
+       help
+         This driver adds support for using IDT CLOCKMATRIX(TM) as a PTP
+         clock. This clock is only useful if your time stamping MAC
+         is connected to the IDT chip.
+
+         To compile this driver as a module, choose M here: the module
+         will be called ptp_clockmatrix.
+
+config PTP_1588_CLOCK_IDT82P33
+       tristate "IDT 82P33xxx PTP clock"
+       select PTP_1588_CLOCK
+       default m
+       help
+         This driver adds support for using the IDT 82P33xxx as a PTP
+         clock. This clock is only useful if your time stamping MAC
+         is connected to the IDT chip.
+
+         To compile this driver as a module, choose M here: the module
+         will be called ptp_idt82p33.
+
 endmenu
```

### Makefile
```
driver/ptp/Makefile
 ...
 obj-$(CONFIG_PTP_1588_CLOCK_QORIQ)     += ptp-qoriq.o
 ptp-qoriq-y                            += ptp_qoriq.o
 ptp-qoriq-$(CONFIG_DEBUG_FS)           += ptp_qoriq_debugfs.o
+obj-$(CONFIG_PTP_1588_CLOCK_IDTCM)     += ptp_clockmatrix.o
+obj-$(CONFIG_PTP_1588_CLOCK_IDT82P33)  += ptp_idt82p33.o
 obj-$(CONFIG_PTP_1588_CLOCK_VMW)       += ptp_vmw.o
 obj-$(CONFIG_PTP_1588_CLOCK_OCP)       += ptp_ocp.o
```

## MISC
### Kconfig
```
 menu "Misc devices"
 
+config RSMU
+       tristate "Renesas Synchronization Management Unit (SMU)"
+       depends on MFD_RSMU_I2C || MFD_RSMU_SPI
+       help
+         This option enables support for Renesas SMU, such as Clockmatrix and
+         82P33XXX series. It will be used by Renesas PTP Clock Manager for
+         Linux (pcm4l) software to provide support to GNSS assisted partial
+         timing support (APTS) and other networking timing functions.
+
 config SENSORS_LIS3LV02D
```
### Makefile
```
 obj-$(CONFIG_HISI_HIKEY_USB)   += hisi_hikey_usb.o
 obj-$(CONFIG_HI6421V600_IRQ)   += hi6421v600-irq.o
 obj-$(CONFIG_OPEN_DICE)                += open-dice.o
 obj-$(CONFIG_VCPU_STALL_DETECTOR)      += vcpu_stall_detector.o
+rsmu-objs                      := rsmu_cdev.o rsmu_cm.o rsmu_sabre.o
+obj-$(CONFIG_RSMU)             += rsmu.o
```

# Copy driver files
Copy *.c, *.h, and *.yaml files in ptp-driver-package/linux directory to corresponding linux kernel location.  The Kconfig and Makefile will need manual integration of relevant sections into the existing files (see previous sections).
```
linux
├── Documentation
│   └── devicetree
│       └── bindings
│           └── ptp
│               ├── ptp-idt82p33.yaml
│               └── ptp-idtcm.yaml
├── drivers
│   ├── mfd
│   │   ├── Kconfig
│   │   ├── Makefile
│   │   ├── rsmu_core.c
│   │   ├── rsmu.h
│   │   ├── rsmu_i2c.c
│   │   └── rsmu_spi.c
│   ├── misc
│   │   ├── Kconfig
│   │   ├── Makefile
│   │   ├── rsmu_cdev.c
│   │   ├── rsmu_cdev.h
│   │   ├── rsmu_cm.c
│   │   └── rsmu_sabre.c
│   └── ptp
│       ├── Kconfig
│       ├── Makefile
│       ├── ptp_clockmatrix.c
│       ├── ptp_clockmatrix.h
│       ├── ptp_idt82p33.c
│       └── ptp_idt82p33.h
└── include
    ├── linux
    │   └── mfd
    │       ├── idt82p33_reg.h
    │       ├── idt8a340_reg.h
    │       └── rsmu.h
    └── uapi
        └── linux
            └── rsmu.h
```
