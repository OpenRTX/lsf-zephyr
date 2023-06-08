Overview
********
    This sample is used to demonstrate how to use DVP driver of CSK6 to obtain image data of CMOS OV9655.

Building and Running
********************
    Make sure you have the csk6 boards connected over Jlink debuger.

    The sample can be built and flashed as follows:

.. code-block:: console

   lisa zep build -b csk6012_nano
   lisa zep flash -r jlink

.. note:: The project only support board:

   - csk6011a_nano
   - csk6011a_c3_nano
   - csk6012_nano
   - csk6012_c3_nano

.. note:: Preview camera images using a PC Tools:
   toos/show_image.py

   * 1. Change main.c to enable serial port protocol ``#define USE_SERIAL_PROTOCOL   (1)``
   * 2. Use an additional USB-to-serial adapter to connect to the TX pin of the target board
   * 3. PC Tools See tools.md for help

Sample Output
=============

.. code-block:: console

    - Device name: DVPI
    - Capabilities:
    VYVU width [320; 320; 0] height [240; 240; 0]
    <inf> main: Alloc vide buffer: 153600

    <inf> main: Alloc vide buffer: 153600

    Capture started
    Got frame 0! size: 153600; timestamp 243 ms fps 4

    Got frame 1! size: 153600; timestamp 269 ms fps 38

    Got frame 2! size: 153600; timestamp 323 ms fps 18

    Got frame 3! size: 153600; timestamp 349 ms fps 38

    Got frame 4! size: 153600; timestamp 376 ms fps 37

    Got frame 5! size: 153600; timestamp 403 ms fps 37

    Got frame 6! size: 153600; timestamp 429 ms fps 38
