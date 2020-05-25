#subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat/$(MTK_PLATFORM)/include
#subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/mt-plat
#subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/mt6755
#subdir-ccflags-y += -I$(srctree)/drivers/spi/mediatek/$(MTK_PLATFORM)
#subdir-ccflags-y += -I$(srctree)/drivers/misc/mediatek/include/

subdir-ccflags-y += -DCFG_VEB_MANAGE_INFO_ERASE
#subdir-ccflags-y += -DCFG_PLATFORM_MTK
subdir-ccflags-y += -Werror

#subdir-ccflags-y += -DCFG_MTK6572

subdir-ccflags-y += -DCFG_VEB_OF
subdir-ccflags-y += -DCFG_VEB_DEBUG
subdir-ccflags-y += -DCONFIG_SMARTCARD

obj-y += veb_a5spi.o
veb_a5spi-y := veb_spi.o
veb_a5spi-y += veb_platform.o
veb_a5spi-y += veb_base.o
veb_a5spi-y += veb_crc.o
veb_a5spi-y += veb_sha.o
veb_a5spi-y += veb_smartcard.o

veb_a5spi-y += veb_a5_upgrade.o
#veb_a5spi-y += veb_a5_upgrade_smart.o
