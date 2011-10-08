ifeq ($(BOARD_HAVE_BLUETOOTH_BCM),true)

LOCAL_PATH:= $(call my-dir)

#
# brcm_patchram_plus.c
#

include $(CLEAR_VARS)

ifeq ($(BOARD_WLAN_DEVICE),bcm4330)
LOCAL_CFLAGS += -DBOARD_HAS_BCM4330
endif
LOCAL_SRC_FILES := brcm_patchram_plus.c

LOCAL_MODULE := brcm_patchram_plus

LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)

endif
