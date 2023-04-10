.. listenai_csk6_bm8563:

LISTENAI CSK6 BM8563 RTC
################

Overview
********

Building and Running
********************

This project can be built and executed on as follows:
lisa zep build -b csk6011a_nano
.. zephyr-app-commands::
   :zephyr-app: samples/drivers/bm8563
   :host-os: unix
   :board: csk6011a_nano
   :goals: run
   :compact:


Sample Output
=============

.. code-block:: console

*** Booting Zephyr OS build nand_scanpen_alpha6-5-g539922902e17  ***                                                                    
[00:00:00.007,000] <inf> main: Set RTC Time: 2025-3-20 13:20:45                                                                         
[00:00:00.010,000] <inf> main: Get RTC Time: 2025-3-20 13:20:45                                                                         
[00:00:01.011,000] <inf> main: Get RTC Time: 2025-3-20 13:20:46                                                                         
[00:00:02.012,000] <inf> main: Get RTC Time: 2025-3-20 13:20:47                                                                         
[00:00:03.013,000] <inf> main: Get RTC Time: 2025-3-20 13:20:48                                                                         
[00:00:04.014,000] <inf> main: Get RTC Time: 2025-3-20 13:20:49                                                                         
[00:00:05.015,000] <inf> main: Get RTC Time: 2025-3-20 13:20:50                                                                         
[00:00:06.016,000] <inf> main: Get RTC Time: 2025-3-20 13:20:51                                                                         
[00:00:07.017,000] <inf> main: Get RTC Time: 2025-3-20 13:20:52                                                                         
[00:00:08.018,000] <inf> main: Get RTC Time: 2025-3-20 13:20:53                                                                         
[00:00:09.019,000] <inf> main: Get RTC Time: 2025-3-20 13:20:54                                                                         
[00:00:10.020,000] <inf> main: Get RTC Time: 2025-3-20 13:20:55                                                                         
[00:00:11.021,000] <inf> main: Get RTC Time: 2025-3-20 13:20:56                                                                         
[00:00:12.022,000] <inf> main: Get RTC Time: 2025-3-20 13:20:57                                                                         
[00:00:13.023,000] <inf> main: Get RTC Time: 2025-3-20 13:20:58                                                                         
[00:00:14.024,000] <inf> main: Get RTC Time: 2025-3-20 13:20:59     