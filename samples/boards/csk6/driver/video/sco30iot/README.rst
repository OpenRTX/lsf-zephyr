Overview
********
    This sample is used to demonstrate how to use DVP driver of CSK6 to obtain image data of CMOS GC032A.

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

    *** Booting Zephyr OS build zephyr-v3.1.0-983-gbe43b0fee9b8  ***
    - Device name: DVPI
    - Capabilities:
    VYUY width [640; 640; 0] height [480; 480; 0]
    [00:00:03.012,000] <inf> main: Alloc vide buffer: 614400

    [00:00:03.012,000] <inf> main: Alloc vide buffer: 614400

    Capture started
    Got frame 0! size: 614400; timestamp 3062 ms fps 0

    Got frame 1! size: 614400; timestamp 3110 ms fps 20

    Got frame 2! size: 614400; timestamp 3158 ms fps 20

    Got frame 3! size: 614400; timestamp 3206 ms fps 20

    Got frame 4! size: 614400; timestamp 3254 ms fps 20

    Got frame 5! size: 614400; timestamp 3302 ms fps 20

    Got frame 6! size: 614400; timestamp 3350 ms fps 20

    Got frame 7! size: 614400; timestamp 3397 ms fps 21

    Got frame 8! size: 614400; timestamp 3445 ms fps 20

    Got frame 9! size: 614400; timestamp 3493 ms fps 20

    Got frame 10! size: 614400; timestamp 3541 ms fps 20
