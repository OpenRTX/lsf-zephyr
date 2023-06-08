.. _UART driver polling samples:

UART driver polling samples
###########

Overview
********

需要注意配置：
将需要使用的串口 属性中的 status 设置成 okay 启用串口

&pinctrl {
	pinctrl_uart0_rx_default: uart0_rx_default{
		pinctrls = <UART0_RXD_GPIOA_02>;              //rx pin
	};

	pinctrl_uart0_tx_default: uart0_tx_default{
		pinctrls = <UART0_TXD_GPIOA_03>;              //tx pin
	};
};



轮寻方式，收到什么返回什么

Building and Running
********************

lisa zep build -b csk6002_9s_nano

lisa zep flash
