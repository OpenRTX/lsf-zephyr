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
[00:00:00.011,000] <inf> main: Get RTC Time: 2025-3-20 13:20:45                                                                         
[00:00:01.012,000] <inf> main: Get RTC Time: 2025-3-20 13:20:46                                                                         
[00:00:02.013,000] <inf> main: Get RTC Time: 2025-3-20 13:20:47                                                                         
[00:00:03.014,000] <inf> main: Get RTC Time: 2025-3-20 13:20:48      