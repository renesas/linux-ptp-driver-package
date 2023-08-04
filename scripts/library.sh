#!/bin/bash

clean_driver_ptp_Kconfig()
{
    TGT_FILE=$1/Kconfig
    echo ${FUNCNAME[0]}:
    echo '    TGT_FILE =' ${TGT_FILE}

    # Delete text between 'config PTP_1588_CLOCK_IDT82P33' to 'ptp_idt82p33.' inclusive
    sed -i '/^config PTP_1588_CLOCK_IDT82P33/,/\(.*ptp_idt82p33\)\./d' $TGT_FILE

    # Delete text between 'config PTP_1588_CLOCK_IDTCM' to 'ptp_clockmatrix.' inclusive
    sed -i '/^config PTP_1588_CLOCK_IDTCM/,/\(.*ptp_clockmatrix\)\./d' $TGT_FILE
    
    # Delete extra blank lines caused by above
    sed -i ':a; /^\n*$/{ s/\n//; N;  ba};' $TGT_FILE
}

insert_driver_ptp_Kconfig()
{
    SRC_FILE=$1/Kconfig
    TGT_FILE=$2/Kconfig

    echo ${FUNCNAME[0]}:
    echo '    SRC_FILE =' ${SRC_FILE}
    echo '    TGT_FILE =' ${TGT_FILE}

    # Extract Kconfig to file
    sed -n "/^config PTP_1588_CLOCK_IDTCM/,/endmenu/p" $SRC_FILE > renesas_temp.txt
    # Delete 'endmenu', ie. the last line
    sed -i '$d' renesas_temp.txt

    # Insert renesas_temp.txt file contents before 'endmenu'
    sed -i '/endmenu/e cat renesas_temp.txt' $TGT_FILE

    # Remove renesas_temp.txt
    rm -f renesas_temp.txt
}

remove_deprecated_files()
{
    TGT_DIR=$1
    
    echo ${FUNCNAME[0]}:
    echo '    TGT_DIR =' ${TGT_DIR}

    FILES="drivers/mfd/rsmu_private.h drivers/ptp/idt8a340_reg.h drivers/ptp/ptp_idt_debugfs.h drivers/ptp/8a3xxxx_reg.h"

    for FILE in $FILES
    do
        rm -f $TGT_DIR/$FILE
        echo "rm -f $TGT_DIR/$FILE"
    done

    # Remove Deprecated Renesas PTP driver files (relative to root of linux kernel)
    rm -f $TGT_DIR/drivers/mfd/rsmu_private.h
    rm -f $TGT_DIR/drivers/ptp/idt8a340_reg.h
    rm -f $TGT_DIR/drivers/ptp/ptp_idt_debugfs.h
    rm -f $TGT_DIR/drivers/ptp/8a3xxxx_reg.h

    echo ""
}

clean_driver_ptp_Makefile()
{
    TGT_FILE=$1/Makefile
    echo ${FUNCNAME[0]}:
    echo '    TGT_FILE =' ${TGT_FILE}

    sed -i '/CONFIG_PTP_1588_CLOCK_IDTCM/d' $TGT_FILE
    sed -i '/CONFIG_PTP_1588_CLOCK_IDT82P33/d' $TGT_FILE
}

insert_driver_ptp_Makefile()
{
    SRC_FILE=$1/Makefile
    TGT_FILE=$2/Makefile

    echo ${FUNCNAME[0]}:
    echo '    SRC_FILE =' ${SRC_FILE}
    echo '    TGT_FILE =' ${TGT_FILE}

    # Extract Makefile to file
    sed -n "/CONFIG_PTP_1588_CLOCK_IDTCM/,/CONFIG_PTP_1588_CLOCK_IDT82P33/p" $SRC_FILE > renesas_temp.txt

    # Insert renesas_temp.txt file contents after 'ptp.o'
    sed -i '/ptp\.o/ r renesas_temp.txt' $TGT_FILE

    # Remove renesas_temp.txt
    rm -f renesas_temp.txt
}

clean_driver_mfd_Makefile()
{
    TGT_FILE=$1/Makefile
#    echo ${FUNCNAME[0]}:
    echo '    TGT_FILE =' ${TGT_FILE}

    sed -i '/rsmu-i2c/d' $TGT_FILE
    sed -i '/rsmu-spi/d' $TGT_FILE

    # Delete double blank lines
    sed -i ':a; /^\n*$/{ s/\n//; N;  ba};' $TGT_FILE

}

insert_driver_mfd_Makefile()
{
    SRC_FILE=$1/Makefile
    TGT_FILE=$2/Makefile

    echo ${FUNCNAME[0]}:
    echo '    SRC_FILE =' ${SRC_FILE}
    echo '    TGT_FILE =' ${TGT_FILE}

    # Extract Makefile to file
    sed -n "/rsmu-i2c-objs/,/rsmu-spi\.o/p" $SRC_FILE > renesas_temp.txt

    # Insert blank line at beginning
    sed -i '1 s/^/\n/' renesas_temp.txt

    # Insert renesas_temp.txt file contents after 'mfd-core.o'
    sed -i '/mfd-core\.o/ r renesas_temp.txt' $TGT_FILE

    # Remove renesas_temp.txt
    rm -f renesas_temp.txt
}

clean_driver_mfd_Kconfig()
{
    TGT_FILE=$1/Kconfig
    echo ${FUNCNAME[0]}:
    echo '    TGT_FILE =' ${TGT_FILE}

    # Delete text between 'MFD_RSMU_I2C' to 'of the device.' inclusive
    sed -i '/^config MFD_RSMU_I2C/,/\(.*of the device\)\./d' $TGT_FILE

    # Delete text between 'config MFD_RSMU_SPI' to 'of the device.' inclusive
    sed -i '/^config MFD_RSMU_SPI/,/\(.*of the device\)\./d' $TGT_FILE

    # Delete double blank lines
    sed -i ':a; /^\n*$/{ s/\n//; N;  ba};' $TGT_FILE
}

insert_driver_mfd_Kconfig()
{
    SRC_FILE=$1/Kconfig
    TGT_FILE=$2/Kconfig

    echo ${FUNCNAME[0]}:
    echo '    SRC_FILE =' ${SRC_FILE}
    echo '    TGT_FILE =' ${TGT_FILE}

    # Extract Kconfig to file
    sed -n "/^config MFD_RSMU_I2C/,/endif/p" $SRC_FILE > renesas_temp.txt
    # Insert blank line at beginning
    sed -i '1 s/^/\n/' renesas_temp.txt 
    # Delete 'endif', ie. the last line
    sed -i '$d' renesas_temp.txt
    # Delete 'blank line', ie. the last line
    sed -i '$d' renesas_temp.txt

    # Insert renesas_temp.txt file contents after 1st occurence of 'default'
    sed -i -e '0,/default/{/default/r renesas_temp.txt' -e '}' $TGT_FILE

    # Remove renesas_temp.txt
    rm -f renesas_temp.txt
}

clean_driver_misc_Makefile()
{
    TGT_FILE=$1/Makefile
    echo ${FUNCNAME[0]}:
    echo '    TGT_FILE =' ${TGT_FILE}

    sed -i '/rsmu-objs/d' $TGT_FILE
    sed -i '/CONFIG_RSMU/d' $TGT_FILE
}

insert_driver_misc_Makefile()
{
    SRC_FILE=$1/Makefile
    TGT_FILE=$2/Makefile

    echo ${FUNCNAME[0]}:
    echo '    SRC_FILE =' ${SRC_FILE}
    echo '    TGT_FILE =' ${TGT_FILE}

    # Extract Makefile to file
    sed -n "/rsmu-objs/,/rsmu\.o/p" $SRC_FILE > renesas_temp.txt

    # Append renesas_temp.txt
    sed -i '$r renesas_temp.txt' $TGT_FILE

    # Remove renesas_temp.txt
    rm -f renesas_temp.txt
}

clean_driver_misc_Kconfig()
{
    TGT_FILE=$1/Kconfig
    echo ${FUNCNAME[0]}:
    echo '    TGT_FILE =' ${TGT_FILE}

    # Delete text between 'onfig RSMU' to 'iming support (APTS) and other networking timing functions.' inclusive
    sed -i '/^config RSMU/,/\(.*timing support (APTS) and other networking timing functions\)\./d' $TGT_FILE

    # Delete extra blank lines caused by above
    sed -i ':a; /^\n*$/{ s/\n//; N;  ba};' $TGT_FILE
}

insert_driver_misc_Kconfig()
{
    SRC_FILE=$1/Kconfig
    TGT_FILE=$2/Kconfig

    echo ${FUNCNAME[0]}:
    echo '    SRC_FILE =' ${SRC_FILE}
    echo '    TGT_FILE =' ${TGT_FILE}

    # Extract Kconfig to file
    sed -n "/^config RSMU/,/endmenu/p" $SRC_FILE > renesas_temp.txt
    # Insert blank line at beginning
    sed -i '1 s/^/\n/' renesas_temp.txt 
    # Delete 'endmenu', ie. the last line
    sed -i '$d' renesas_temp.txt
    # Delete 'blank line', ie. the last line
    sed -i '$d' renesas_temp.txt

    # Insert renesas_temp.txt file contents after 'menu "Misc devices"'
    sed -i '/menu "\Misc devices\"/ r renesas_temp.txt' $TGT_FILE

    # Remove renesas_temp.txt
    rm -f renesas_temp.txt
}

copy_files()
{
    SRC=$1
    DST=$2
    FILES=$3

    for FILE in $FILES
    do
        rm -f $DST/$FILE
        cp $SRC/$FILE $DST/$FILE
        echo "cp $SRC/$FILE $DST/$FILE"
    done

    echo ""
}