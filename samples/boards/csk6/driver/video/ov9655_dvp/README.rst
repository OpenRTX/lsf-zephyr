Overview
********
    This sample is used to demonstrate how to use DVP driver of CSK6 to obtain image data of CMOS OV9655.

Building and Running
********************
    Make sure you have the csk6 boards connected over Jlink debuger.

    The sample can be built and flashed as follows:

.. code-block:: console

   lisa zep build -b csk6002_9s_nano
   lisa zep flash -r jlink

Sample Output
=============

.. code-block:: console

    - Device name: DVPI
    - Capabilities:
    VYVU width [320; 320; 0] height [240; 240; 0]
    <inf> main: Alloc vide buffer: 153600

    <inf> main: Alloc vide buffer: 153600

    Capture started
    Got frame 0! size: 153600; timestamp 211 ms fps 4

    Got frame 1! size: 153600; timestamp 227 ms fps 62

    Got frame 2! size: 153600; timestamp 259 ms fps 31

    Got frame 3! size: 153600; timestamp 275 ms fps 62

    Got frame 4! size: 153600; timestamp 291 ms fps 62

    Got frame 5! size: 153600; timestamp 307 ms fps 62

    Got frame 6! size: 153600; timestamp 323 ms fps 62

    Got frame 7! size: 153600; timestamp 339 ms fps 62