
CSK6 I2C Master/Slave transimit/recieve
##########################################

Overview
********

This sample demonstrates how to use CSK6 i2c peripheral to transimit/recieve data.

Requirements
************

This sample use two group i2c pins which i2c0(GPIO_A_10, GPIO_A_11) is as master, 
i2c1(GPIO_A_13, GPIO_A_12) as slave. 

make sure to connect GPIO_A_10 to GPIO_A_13, GPIO_A_11 to GPIO_A_12 all with pull-up resistor.

Building and Running
********************

Make sure you have the csk6 boards connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console

   west build -b csk6_xxx samples/boards/csk6/drivers/i2c/master_slave
   west flash

Sample Output
=============

.. code-block:: console

    *** Booting Zephyr OS build ab4367d4514a  ***
    I2C START TEST
    i2c thread creaeted
    master send, slave receive
    master receive, slave send
    I2C TEST DONE
