#!/bin/bash
# set -x

echo "SRC_DIR = $1"
echo "DST_DIR = $2"

SRC_DIR=$1
DST_DIR=$2
echo ""

. $(dirname "$0")/library.sh

echo Remove deprecated files
echo =======================
remove_deprecated_files $DST_DIR

echo Documentation/devicetree/bindings/ptp
echo =====================================
TARGET=Documentation/devicetree/bindings/ptp

copy_files $SRC_DIR/linux/$TARGET \
           $DST_DIR/$TARGET \
           "ptp-idt82p33.yaml ptp-idtcm.yaml"

echo MFD
echo ===
TARGET=drivers/mfd

SRC=$SRC_DIR/linux/$TARGET
DST=$DST_DIR/$TARGET

clean_driver_mfd_Makefile $DST
insert_driver_mfd_Makefile $SRC $DST

clean_driver_mfd_Kconfig $DST
insert_driver_mfd_Kconfig $SRC $DST

copy_files $SRC $DST "rsmu.h rsmu_core.c rsmu_i2c.c rsmu_spi.c"

TARGET=include/linux/mfd
copy_files $SRC_DIR/linux/$TARGET \
           $DST_DIR/$TARGET \
           "idt82p33_reg.h idt8a340_reg.h rsmu.h"


echo PTP
echo ===
TARGET=drivers/ptp

SRC=$SRC_DIR/linux/$TARGET
DST=$DST_DIR/$TARGET

clean_driver_ptp_Kconfig $DST
insert_driver_ptp_Kconfig $SRC $DST

clean_driver_ptp_Makefile $DST
insert_driver_ptp_Makefile $SRC $DST

copy_files $SRC $DST "ptp_clockmatrix.c ptp_clockmatrix.h ptp_idt82p33.c ptp_idt82p33.h"

echo MISC
echo ====
TARGET=drivers/misc

SRC=$SRC_DIR/linux/$TARGET
DST=$DST_DIR/$TARGET

clean_driver_misc_Makefile $DST
insert_driver_misc_Makefile $SRC $DST

clean_driver_misc_Kconfig $DST
insert_driver_misc_Kconfig $SRC $DST

copy_files $SRC $DST "rsmu_cdev.c rsmu_cdev.h rsmu_cm.c rsmu_sabre.c"

TARGET=include/uapi/linux
copy_files $SRC_DIR/linux/$TARGET \
           $DST_DIR/$TARGET \
           "rsmu.h"
