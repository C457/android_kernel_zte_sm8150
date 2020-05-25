ifeq ($(ENABLE_AB),false)
        dtbo-y = sm8150-sdx50m-mtp-$(ZTE_BOARD_NAME)-non-ab-overlay.dtbo
        sm8150-sdx50m-mtp-$(ZTE_BOARD_NAME)-non-ab-overlay.dtbo-base := sm8150-v2-$(ZTE_BOARD_NAME).dtb
        $(warning ZTE_FEATURE_NON_AB_SYSTEM has defined)
else
        dtbo-y = sm8150-sdx50m-mtp-$(ZTE_BOARD_NAME)-overlay.dtbo
        sm8150-sdx50m-mtp-$(ZTE_BOARD_NAME)-overlay.dtbo-base := sm8150-v2-$(ZTE_BOARD_NAME).dtb
        $(warning ZTE_FEATURE_NON_AB_SYSTEM has not defined)
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
