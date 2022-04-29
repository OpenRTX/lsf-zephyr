.. _wifi_sta:

CSK6 WiFi Station
####################

Overview
********

This sample demonstrates how to use csk6 to connect to a WiFi network as a station device.

Building and Running
********************

Make sure you have the CSK6 connected over USB port.

The sample can be built and flashed as follows:

.. code-block:: console

   west build -b csk6 samples/boards/csk6/wifi_station
   west flash

Sample Output
=============

To check output of this sample, any serial console program can be used (i.e. on Linux minicom, putty, screen, etc)
This example uses ``minicom`` on the serial port ``/dev/ttyUSB0``:

.. code-block:: console

   $ minicom -D /dev/ttyUSB0 -b 115200

.. code-block:: console

    *** Booting Zephyr OS build f4ee5f98ad0b  ***
    connecting to wifi: TP-LINK_LINGSI ...
    [WiFi sta] connected
    Your address: 192.168.1.197
    Lease time: 7200 seconds
    Subnet: 255.255.255.0
    Router: 192.168.1.1
