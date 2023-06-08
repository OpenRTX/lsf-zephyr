.. _UART driver interrupt samples:

UART driver interrupt samples
###########

Overview
********

需要注意配置：
将需要使用的串口 属性中的status设置成okay 启用串口

zephyr/boards/arm/csk6002_9s_nano/csk6002_9s_nano.dts
&uart0 {
        pinctrl-0 = <&pinctrl_uart0_rx_default &pinctrl_uart0_tx_default>; 
        pinctrl-names = "default";
        current-speed = <115200>;               // 波特率
        status = "okay";
};

boards/csk6002_9s_nano.overlay
/* UART alternate function */
pinctrl_uart0_rx_default: uart0_rx_default{
        pinctrls = <&pinmuxa 2 2>;              //rx pin
};

pinctrl_uart0_tx_default: uart0_tx_default{
        pinctrls = <&pinmuxa 3 2>;              //tx pin
};


中断方式，收到什么返回什么

Building and Running
********************

lisa zep build -b csk6002_9s_nano

lisa zep flash