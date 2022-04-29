.. _scanpen_samples:

CSK6012 9S Scanpen samples
###########

Overview
********

本Sample为scanpen的硬件驱动使用示例合集。

目前已集成：
1. LCD驱动
#. LCD_PWM驱动
#. 触摸屏驱动
#. CAM_LED驱动
#. Keysense驱动
#. ADC驱动
#. pwr_lock 上电拉高锁定


ADC检测按键

.. code-block:: console
   analogkey-trigger = <868 1890 2570>; // 根据adc检测值设置



注意：因为LCD_PWM需要使用到芯片SDIO PIN配置为PWM功能进行驱动，所以上电5s后将不可下载程序。

Building and Running
********************

The sample can be built and flashed as follows:

.. code-block:: console
   lisa zep build -b csk6012_9s_scanpen
   lisa zep flash

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build 3d7257d84a3c  ***
   [00:00:00.153,000] <inf> sample: 下载模式,5s后变为PWM模式不可下载程序，重新下载需要上电并在5s内进行下载
   [00:00:00.153,000] <inf> sample: ADC Sample start
   [00:00:00.153,000] <inf> sample: adc_vref 3300.
   [00:00:00.153,000] <inf> sample:  adc(0)= -26400 mV
   [00:00:01.453,000] <inf> sample:  adc(2455)= 5240 mV
   [00:00:02.753,000] <inf> sample:  adc(2456)= 5256 mV
   [00:00:04.053,000] <inf> sample:  adc(2464)= 5360 mV
   [00:00:05.153,000] <inf> sample: PWM模式
   [00:00:05.153,000] <inf> sample: csk6 keysense sample application
   [00:00:05.153,000] <inf> sample: key_dev init success!
   [00:00:05.353,000] <inf> sample:  adc(2464)= 5360 mV
   [00:00:06.654,000] <inf> sample:  adc(2455)= 5240 mV
   [00:00:07.954,000] <inf> sample:  adc(2455)= 5240 mV
   [00:00:09.254,000] <inf> sample:  adc(2455)= 5240 mV



