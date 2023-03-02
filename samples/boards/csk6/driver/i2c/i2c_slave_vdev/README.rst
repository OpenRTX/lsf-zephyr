I2C Slave Virtual Device Sample
##################

I2C Master Operation
=============

.. code-block:: console

   i2c-tools> i2cset -c 0x32 -r 0x10 0x11 0x12 0x13 0x14 0x15 0x45 0x47
   I (8259845) cmd_i2ctools: Write OK
   i2c-tools> i2cset -c 0x32 -r 0x10 0x11 0x12 0x13
   I (8267105) cmd_i2ctools: Write OK
   i2c-tools> i2cget -c 0x32 -r 0x10 -l 2
   0x11 0xac
   i2c-tools> i2cget -c 0x32 -r 0x10 -l 1
   0x11
   i2c-tools> i2cget -c 0x32 -r 0x12 -l 1
   0x13
   i2c-tools>



CSK6 Slave Console Output
=============

.. code-block:: console

   *** Booting Zephyr OS build nand_scanpen_alpha6-4-gfb9f8c565287  ***       
   I2C Slave Virtual device Note: SLAVE_VDEV_0                                
   Device: SLAVE_VDEV_0, I2C Addr: 0x32                                       
                                                                              
   [00:00:00.014,000] <inf> slave_vdev: start_slave_vdev_handler_task ok      
   uart:~$ I2C Receive data length:8                                          
   I2C Receive data length:4                                                  
   I2C Receive data length:1                                                  
   I2C Receive data length:1                                                  
   I2C Receive data length:1 