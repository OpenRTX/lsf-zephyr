CSK6 dma memory copy
#######################

Overview
********

This sample demonstrates how to use CSK6 dma(working in Memory to Memory mode) to perform a `memcpy()`-like manner.
And give a speed comparison between in standard lib `memcpy()` and dma copy.

Building and Running
********************

Make sure you have the CSK6 connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console

   west build -b csk6 samples/boards/csk6/dma_memcpy
   west flash

Sample Output
=============

To check output of this sample, any serial console program can be used (i.e. on Linux minicom, putty, screen, etc)
This example uses ``minicom`` on the serial port ``/dev/ttyUSB0``:

.. code-block:: console

   $ minicom -D /dev/ttyUSB0 -b 115200

.. code-block:: console

    *** Booting Zephyr OS build 60a5bbd0f655  ***

    DMA copy start
    DMA copy cost time: 204 us
    DMA copy finish

    STD memcpy start
    STD memecpy cost time: 170 us
    STD memcpy finish

    DMA copy start
    DMA copy cost time: 112 us
    DMA copy finish

    STD memcpy start
    STD memecpy cost time: 164 us
    STD memcpy finish

    DMA copy start
    DMA copy cost time: 104 us
    DMA copy finish

    STD memcpy start
    STD memecpy cost time: 164 us
    STD memcpy finish
