CSK6 spi master
##################

Overview
********

This sample demonstrates how to use CSK6 spi master peripheral to transimit/recieve data.
This sample enable spi hardware Chip-Select function in default, users could disable it by defining
an overlay file that rewrite the spi node and remove the ``cs-gpios`` item, and then edit the menuconfig
to enable the software Chip-Select function and select a gpio pin as the cs pin.

Building and Running
********************

Make sure you have the CSK6 connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console

   west build -b csk6 samples/boards/csk6/spi
   west flash

Sample Output
=============

To check output of this sample, any serial console program can be used (i.e. on Linux minicom, putty, screen, etc)
This example uses ``minicom`` on the serial port ``/dev/ttyUSB0``:

.. code-block:: console

   $ minicom -D /dev/ttyUSB0 -b 115200

.. code-block:: console

    *** Booting Zephyr OS build 6d9b2a8c2aee  ***
    spi master example
    spi master sending data ...
    spi master sending data ...
    spi master sending data ...
    spi master sending data ...

