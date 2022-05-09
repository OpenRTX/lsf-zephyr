.. _wifi_sta:

CSK6 WiFi Station
####################

Overview
********

This sample demonstrates how to use csk6 to scan neighbouring WiFi AP devices and get its info.

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

   *** Booting Zephyr OS build 7edc651bcfb7  ***
   ssid:szr-hotel  bssid:88:25:93:bd:f3:07  rssi: -56  channel: 7
   ssid:szr-hotel  bssid:88:25:93:bd:f5:53  rssi: -37  channel: 9
   ssid:szr-hotel  bssid:88:25:93:bd:f7:b8  rssi: -58  channel: 8
   ssid:szr-hotel  bssid:88:25:93:bd:f5:70  rssi: -66  channel: 5
   ssid:szr-hotel  bssid:88:25:93:bd:f4:9a  rssi: -74  channel: 1
   ssid:szr-hotel  bssid:88:25:93:bd:f6:b7  rssi: -74  channel: 12
   ssid:szr-hotel  bssid:88:25:93:bd:f8:bd  rssi: -74  channel: 13
   ssid:szr-hotel  bssid:88:25:93:bd:f7:cb  rssi: -80  channel: 8
   scan done

