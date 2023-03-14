.. _infrared-sample:

Infrared Info Sample
#################################

Overview
********

This sample demonstrates how to use the infrared driver API.


Building and Running
********************

Make sure that the CONFIG is enabled (``CONFIG_INFRARED=y``).


Building and Running
=========================================


Sample output
=============

.. code-block:: console
   uart:~$ *** Booting Zephyr OS build v1.2.1-alpha.3-1-gd53bd064e5d7  ***


   [00:00:00.006,000] <inf> Infrared: IR Sample Start
   [00:00:00.006,000] <inf> Infrared: 1.IR Send
   [00:00:00.006,000] <inf> Infrared: 2.IR Send
   [00:00:00.106,000] <inf> Infrared: 3.IR Rece
   [00:00:03.553,000] <inf> Infrared: Received IR signal with address 0xFF00 and command 0xB847
   [00:00:03.606,000] <inf> Infrared: Received IR signal with address 0xFF00 and command 0xB847
   [00:00:03.714,000] <inf> Infrared: Received IR signal with address 0xFF00 and command 0xB847
   [00:00:03.930,000] <inf> Infrared: Received IR signal with address 0xFF00 and command 0xB847
   [00:00:04.038,000] <inf> Infrared: Received IR signal with address 0xFF00 and command 0xB847
   uart:~$


