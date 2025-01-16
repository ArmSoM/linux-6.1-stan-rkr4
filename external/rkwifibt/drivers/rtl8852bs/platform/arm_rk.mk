ifeq ($(CONFIG_PLATFORM_ARM_ROCKCHIP), y)
EXTRA_CFLAGS += -DCONFIG_LITTLE_ENDIAN 
EXTRA_CFLAGS += -DCONFIG_IOCTL_CFG80211 -DRTW_USE_CFG80211_STA_EVENT
#EXTRA_CFLAGS += -DCONFIG_RADIO_WORK
EXTRA_CFLAGS += -DCONFIG_CONCURRENT_MODE
#ifeq ($(shell test $(CONFIG_RTW_ANDROID) -ge 11; echo $$?), 0)
#EXTRA_CFLAGS += -DCONFIG_IFACE_NUMBER=2
#EXTRA_CFLAGS += -DCONFIG_PLATFORM_ROCKCHIPS
#endif

ARCH := arm64
CROSS_COMPILE := /home/alientek/rk3568/prebuilts/gcc/linux-x86/aarch64/gcc-arm-10.3-2021.07-x86_64-aarch64-none-linux-gnu/bin/aarch64-none-linux-gnu-
KSRC := /home/alientek/rk3568/kernel

ifeq ($(CONFIG_SDIO_HCI), y)
_PLATFORM_FILES = platform/platform_ARM_RK_sdio.o
endif
endif