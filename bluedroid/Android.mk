#
# libbluedroid
#

LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

ifeq ($(BOARD_CUSTOM_BLUEDROID),)
	LOCAL_SRC_FILES := \
		bluetooth.c
else
	LOCAL_SRC_FILES := \
		$(BOARD_CUSTOM_BLUEDROID)
endif

LOCAL_C_INCLUDES := \
	$(LOCAL_PATH)/include \
	system/bluetooth/bluez-clean-headers

LOCAL_SHARED_LIBRARIES := \
	libcutils

LOCAL_MODULE := libbluedroid

include $(BUILD_SHARED_LIBRARY)
