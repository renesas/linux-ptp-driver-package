#
# Makefile for multifunction miscellaneous devices
#

obj-$(CONFIG_MFD_CORE)		+= mfd-core.o

rsmu-i2c-objs			:= rsmu_core.o rsmu_i2c.o
rsmu-spi-objs			:= rsmu_core.o rsmu_spi.o
obj-$(CONFIG_MFD_RSMU_I2C)	+= rsmu-i2c.o
obj-$(CONFIG_MFD_RSMU_SPI)	+= rsmu-spi.o

