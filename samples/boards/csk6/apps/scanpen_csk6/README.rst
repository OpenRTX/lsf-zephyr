Startup Sample Application
##########################

Overview
********

This application mainly shows the system startup process and firmware update 
related configuration.
Before running this example, you first need to package the second core firmware into the file system 
and burn it to the corresponding Flash area address.

Building the application for csk6001
***********************************************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6001/apps/startup/
   :board: csk6001
   :goals: debug

Open a serial terminal (minicom, putty, etc.) and connect the board with the
following settings:

- Speed: 115200
- Data: 8 bits
- Parity: None
- Stop bits: 1

.. code-block:: console

*** Booting Zephyr OS build 0a2f8207a5d2  ***
I: Starting bootloader
I: Primary image: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3
I: Scratch: magic=unset, swap_type=0x1, copy_done=0x3, image_ok=0x3
I: Boot source: primary slot
I: Swap type: none
I: Bootloader chainload address offset: 0x40000
I: Jumping to the first image slot
*** Booting Zephyr OS build 0a2f8207a5d2  ***
metal: debug:     registered generic bus
/lfs1 automounted
/lfs1: bsize = 16 ; frsize = 4096 ; blocks = 1536 ; bfree = 530
/lfs1 opendir: 0
  F 11024 029_music_not_found_16k.mp3
  F 71056 cp_fw.bin
  F 4014860 venus_cp_fw.bin
End of files
/lfs1 closedir
csk6001_rproc_init: node id: 2
Step1: Create rproc success!
/lfs1/cp_fw.bin stat: 0
Get file 'cp_fw.bin' size 71056
[Upgrade]read size 71056 write to 0x30000000
Load second core firmware success!
Step2: load second core firmware success!
----tester loop cnt =0---------
Step3: first jump config second core config success!
[00:00:00.000,000] <inf> littlefs: littlefs partition at /lfs1
[00:00:00.000,000] <inf> littlefs: LittleFS version 2.2, disk version 2.0
[00:00:00.001,000] <inf> littlefs: FS at FLASH_CTRL:0x160000 is 1536 0x1000-byte blocks with 512 cycle
[00:00:00.001,000] <inf> littlefs: sizes: rd 16 ; pr 16 ; ca 64 ; la 32
[00:00:00.002,000] <inf> littlefs: /lfs1 mounted
[00:00:00.002,000] <inf> littlefs: Automount /lfs1 succeeded
*** Booting Zephyr OS build 0a2f8207a5d2  ***
Hello World! ls_adsp_csk6
Step4: start second core success!
[Master]Ctrl remote core goto stop wait (1000) millisecond...
[Master]Ctrl remote core goto stop wait (900) millisecond...
[Master]Ctrl remote core goto stop wait (800) millisecond...
[Master]Ctrl remote core goto stop wait (700) millisecond...
[Master]Ctrl remote core goto stop wait (600) millisecond...
[Master]Ctrl remote core goto stop wait (500) millisecond...
[Master]Ctrl remote core goto stop wait (400) millisecond...
[Master]Ctrl remote core goto stop wait (300) millisecond...
[Master]Ctrl remote core goto stop wait (200) millisecond...
[Master]Ctrl remote core goto stop wait (100) millisecond...
Step5: stop second core success!
Step6: shutdown second core success!
Step7: remove second core success!
======The test success==========
