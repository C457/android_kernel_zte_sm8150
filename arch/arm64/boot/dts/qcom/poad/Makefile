ifeq ($(ENABLE_AB),false)
        dtbo-y = sm8150-sdx50m-mtp-poad-non-ab-overlay.dtbo
        sm8150-sdx50m-mtp-poad-non-ab-overlay.dtbo-base := sm8150-v2-poad.dtb
        $(warning ZTE_FEATURE_NON_AB_SYSTEM has defined)
else
        dtbo-y = sm8150-sdx50m-mtp-poad-overlay.dtbo
        sm8150-sdx50m-mtp-poad-overlay.dtbo-base := sm8150-v2-poad.dtb
        $(warning ZTE_FEATURE_NON_AB_SYSTEM has not defined)
endif

always		:= $(dtb-y)
subdir-y	:= $(dts-dirs)
clean-files	:= *.dtb
