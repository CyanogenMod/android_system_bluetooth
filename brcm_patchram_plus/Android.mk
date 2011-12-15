ifeq ($(BOARD_HAVE_BLUETOOTH_BCM),true)

LOCAL_PATH:= $(call my-dir)

#
# brcm_patchram_plus.c
#

include $(CLEAR_VARS)

ifeq ($(BOARD_WLAN_DEVICE),bcm4330)
LOCAL_CFLAGS += -DBOARD_HAS_BCM4330
endif
ifeq ($(BOARD_HAVE_BLUETOOTH_BCM_SEMC),true)
LOCAL_CFLAGS += -DBCM_SEMC
endif

ifneq ($(BOARD_BRCM_PATCHRAM_PLUS_C),)
  LOCAL_SRC_FILES += $(BOARD_BRCM_PATCHRAM_PLUS_C)
else
  LOCAL_SRC_FILES += brcm_patchram_plus.c
endif

LOCAL_MODULE := brcm_patchram_plus

LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)

endif
