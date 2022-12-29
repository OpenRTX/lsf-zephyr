
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

    *** Booting Zephyr OS build v1.1.1-alpha.2  ***
    I2C START TEST
    I2C Thread creaeted
    [202:i2c_slave_thread]1.I2C0:Master Write, I2C1:Slave Read
    I2C0:Master Write, I2C1:Slave Read
    I2C1:Slave Read data:
    [0x000000] |   05  06  07  08  09  0A  0B  0C  0D  0E  0F  10  11  12  13  14
    [0x000010] |   15  16  17  18  19  1A  1B  1C  1D  1E  1F  20  21  22  23  24
    [0x000020] |   25  26  27  28  29  2A  2B  2C  2D  2E  2F  30  31  32  33  34
    [0x000030] |   35  36  37  38  39  3A  3B  3C  3D  3E  3F  40  41  42  43  44
    [231:i2c_slave_thread]2.I2C1:Slave Write, I2C0:Master Read
    [164:i2c_master_thread]2.I2C1:Slave Write, I2C0:Master Read
    Wait 500ms...
    I2C Salve_Thread Done
    I2C0:Master Read data
    [0x000000] |   00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F
    [0x000010] |   10  11  12  13  14  15  16  17  18  19  1A  1B  1C  1D  1E  1F
    [0x000020] |   20  21  22  23  24  25  26  27  28  29  2A  2B  2C  2D  2E  2F
    [0x000030] |   30  31  32  33  34  35  36  37  38  39  3A  3B  3C  3D  3E  3F
    I2C Master_Thread Done