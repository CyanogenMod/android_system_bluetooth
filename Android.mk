ifeq ($(BOARD_HAVE_BLUETOOTH),true)
  include $(all-subdir-makefiles)
endif

ifdef TARGET_IS_GALAXYS
  LOCAL_CFLAGS += -DTARGET_IS_GALAXYS
endif