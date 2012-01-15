ifeq ($(BOARD_HAVE_BLUETOOTH_BCM),true)

LOCAL_PATH:= $(call my-dir)

#
# brcm_patchram_plus.c
#

include $(CLEAR_VARS)

ifeq ($(BOARD_HAVE_BLUETOOTH_BCM_SEMC),true)
LOCAL_CFLAGS += -DBCM_SEMC
endif

LOCAL_SRC_FILES := brcm_patchram_plus.c

LOCAL_MODULE := brcm_patchram_plus

LOCAL_SHARED_LIBRARIES := libcutils liblog

LOCAL_C_FLAGS := \
	-DANDROID

include $(BUILD_EXECUTABLE)

endif
