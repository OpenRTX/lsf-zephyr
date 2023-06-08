
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

*** Booting Zephyr OS build v1.1.3-alpha.1-5-g8940a4be0ce1  ***
Counter alarm sample

g_counter 0
g_counter 1
g_counter 2
g_counter 3
g_counter 4
g_counter 5
g_counter 6
g_counter 7
g_counter 8
g_counter 9
g_counter 10
restart
g_counter 0
g_counter 1
g_counter 2
g_counter 3
g_counter 4
g_counter 5

