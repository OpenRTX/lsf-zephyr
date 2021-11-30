MODULE_SRC_DIRS := esp-hosted/common \
					port/host_common \
					port/csk4002/hosted \
					port/csk4002/common \
					port/csk4002/driver/network \
					port/csk4002/driver/serial \
					port/csk4002/driver/spi \
					port/csk4002/driver/gpio \
					port/csk4002/driver/vhci \


MODULE_INCLUDE_DIRS := esp-hosted/common/include \
                        port/host_common/include \
                        port/csk4002/hosted/include \
                        port/csk4002/common/include \
                        port/csk4002/driver/netif \
                        port/csk4002/driver/network \
                        port/csk4002/driver/serial \
                        port/csk4002/driver/spi \
                        port/csk4002/driver/gpio \
                        port/csk4002/driver/vhci \

MODULE_CFLAGS += -DLIB_NM=\"$(MODULE_NAME)\" -DCMT_ID=\"SDK\"

MODULE_CFLAGS += -Wno-unused-variable
MODULE_CFLAGS += -Wno-unused-but-set-variable

MODULE_LIBS := debug bsp rtos protocolbuffer esp_event esp_port
