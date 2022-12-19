
CSK6 COUNTER 
##########################################

Overview
********

This sample demonstrates how to use CSK6 COUNTER INTERRUPT

Requirements
************

This sample use two channel in gpt timer

Building and Running
********************

Make sure you have the csk6 boards connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console

   west build -b csk6_csk6011a_nano -p
   west flash

Sample Output
=============

.. code-block:: console

*** Booting Zephyr OS build v1.1.2-alpha.2-18-g7d154a631bd3  ***
Counter alarm sample

counter Channel is: 1
counter Channel is: 0

