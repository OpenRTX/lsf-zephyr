.. _display-sample:

Display Sample
##############

Overview
********

This example is used to calculate the rate at which the SPI interface refreshes a frame of image. 
Each frame size can be statically configured through CONFIG_DISPLAY_X_RES and CONFIG_DISPLAY_Y_RES. 
In addition, the memory location where image data is stored also affects the refresh rate. 
CONFIG_FRAME_MEM_PSRAM/CONFIG_FRAME_MEM_SRAM can be used to configure the memory space for data

Building and Running
********************

Make sure you have the csk6 boards connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console

   lisa zep build -b csk6002_9s_nano
   lisa zep flash

Sample Output
=============

.. code-block:: console

    *** Booting Zephyr OS build v1.0.2-beta.1-46-g6b4fbe82d5bc  ***
    I: Display sample for DISPLAY
    I: display capabilities: x_resolution 320  y_resolution 480  format 1

    I: display frame: xy (320 480) depth 3 byte, period: 124427 us, write fps: 8 

    I: display frame: xy (320 480) depth 3 byte, period: 124359 us, write fps: 8 

    I: display frame: xy (320 480) depth 3 byte, period: 124352 us, write fps: 8 


