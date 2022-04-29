.. _adc-sample:

Analog-to-Digital Converter (ADC)
#################################

Overview
********

This sample demonstrates how to use the ADC driver API.


Building and Running
********************

The ADC peripheral and pinmux is configured in the board's ``.dts`` file. Make
sure that the ADC is enabled (``status = "okay";``).

In addition to that, this sample requires an ADC channel specified in the
``io-channels`` property of the ``zephyr,user`` node. This is usually done with
a devicetree overlay. 

Building and Running 
=========================================


Sample output
=============


