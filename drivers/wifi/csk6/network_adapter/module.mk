MODULE_SRC_DIRS := .

MODULE_INCLUDE_DIRS := include

MODULE_CFLAGS += -DLIB_NM=\"$(MODULE_NAME)\" -DCMT_ID=\"SDK\"

MODULE_CFLAGS += -Wno-unused-variable
MODULE_CFLAGS += -Wno-unused-but-set-variable

MODULE_LIBS := rtos lwip esp_hosted debug esp_event esp_port
