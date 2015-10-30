ifeq ($(BOARD_HAVE_BLUETOOTH_BCM),true)

ifdef TARGET_IS_GALAXYS
  LOCAL_CFLAGS += -DTARGET_IS_GALAXYS
endif

LOCAL_PATH:= $(call my-dir)

#
# brcm_patchram_plus.c
#

include $(CLEAR_VARS)

LOCAL_SRC_FILES := brcm_patchram_plus.c

LOCAL_MODULE := brcm_patchram_plus

LOCAL_SHARED_LIBRARIES := libcutils

include $(BUILD_EXECUTABLE)

endif
