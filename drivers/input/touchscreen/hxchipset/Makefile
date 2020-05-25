# Makefile for the Himax touchscreen drivers.
ccflags-y += -I$(srctree)/drivers/input/touchscreen/hxchipset/$(CONFIG_TS_FIRMWARE_PALM)/
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INCELL)   			+=	himax_ic_incell_core.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_IC_HX83112)   		+=	himax_ic_HX83112.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_COMMON)   			+=	himax_common.o himax_platform.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_DEBUG)   			+=	himax_debug.o
obj-$(CONFIG_TOUCHSCREEN_HIMAX_INSPECT)   			+=	himax_inspection.o
obj-y += himax_common_interface.o
