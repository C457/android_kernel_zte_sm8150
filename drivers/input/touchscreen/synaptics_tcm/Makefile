#
# Makefile for the Synaptics TCM touchscreen driver.
#

# Each configuration option enables a list of files.
ccflags-y += -I$(srctree)/drivers/input/touchscreen/synaptics_tcm/firmware_config/$(CONFIG_TS_FIRMWARE)/
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_SPI) += synaptics_tcm_spi.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_I2C) += synaptics_tcm_i2c.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_CORE) += synaptics_tcm_core.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_TOUCH) += synaptics_tcm_touch.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_TOUCH_TMP) += synaptics_tcm_touch_tmp.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_DEVICE) += synaptics_tcm_device.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_TESTING) += synaptics_common_interface.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_TESTING) += synaptics_tcm_testing.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_REFLASH) += synaptics_tcm_reflash.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_RECOVERY) += synaptics_tcm_recovery.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_ZEROFLASH) += synaptics_tcm_zeroflash.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_DIAGNOSTICS) += synaptics_tcm_diagnostics.o
obj-$(CONFIG_TOUCHSCREEN_SYNAPTICS_TCM_ROMBOOT) += synaptics_tcm_romboot.o
obj-$(CONFIG_TP_COMMON_INTERFACE) += synaptics_common_interface.o
