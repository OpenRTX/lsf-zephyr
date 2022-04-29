.. _cdc-acm-console:

Console and Shell over CDC ACM UART Sample
################################

Overview
********

A simple Hello World sample, with console output coming via CDC ACM UART. a shell input is also coming via the same CDC ACM UART.
Primarily intended to show the required config options.

Requirements
************

This project requires a USB device controller driver.

Building and Running
********************


Make sure you have the CSK6 connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console

   west build -b csk6_xxx samples/boards/csk6/subsys/usb/cdc/console_shell
   west flash

Plug the board into a host device, for sample, a PC running Linux OS.
The board will be detected as a CDC_ACM serial device. To see the console output
from the sample, use a command similar to "minicom -D /dev/ttyACM0".

.. code-block:: console

   Hello World! arm
   Hello World! arm

   uart:~$ Hello World! arm
   Hello World! arm
   Hello World! arm
   Hello World! arm
   uart:~$ 
   uart:~$ 
   uart:~$ Hello World! arm
   Hello World! arm
   Hello World! arm
   Hello World! arm
   Hello World! arm
   Hello World! arm

Troubleshooting
===============

You may need to stop modemmanager via "sudo stop modemmanager", if it is
trying to access the device in the background.
