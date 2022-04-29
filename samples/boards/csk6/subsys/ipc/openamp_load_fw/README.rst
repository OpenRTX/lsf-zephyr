.. _penAMP_load_remote_fw_sample:

OpenAMP load fw Sample Application
##########################

Overview
********

This application demonstrates how to use OpenAMP with Zephyr. It is designed to
demonstrate how to integrate OpenAMP with Zephyr both from a build perspective
and code.

Building the application for csk6
***********************************************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/ipc/openamp_load_fw
   :board: csk6
   :goals: debug

Open a serial terminal (minicom, putty, etc.) and connect the board with the
following settings:

- Speed: 115200
- Data: 8 bits
- Parity: None
- Stop bits: 1

Before running this example, you first need to package the remote firmware into the file system 
and burn it to the corresponding Flash area address.
Reset the board and the following message will appear on the corresponding
serial port.

.. code-block:: console

*** Booting Zephyr OS build 2f859ef2055a  ***
[00:00:00.000,000] <inf> littlefs: littlefs partition at /lfs1
[00:00:00.000,000] <inf> littlefs: LittleFS version 2.2, disk version 2.0
[00:00:00.001,000] <inf> littlefs: FS at FLASH_CTRL:0x100000 is 1776 0x1000-byte blocks with 512 cycle
[00:00:00.001,000] <inf> littlefs: sizes: rd 16 ; pr 16 ; ca 64 ; la 32
[00:00:00.003,000] <inf> littlefs: /lfs1 mounted
[00:00:00.003,000] <inf> littlefs: Automount /lfs1 succeeded
metal: debug:     registered generic bus
/lfs1 automounted
/lfs1: bsize = 16 ; frsize = 4096 ; blocks = 1776 ; bfree = 752
/lfs1 opendir: 0
  F 11024 029_music_not_found_16k.mp3
  F 79696 cp_fw.bin
  F 4014860 venus_cp_fw.bin
  F 63152 zephyr_cp_bin - 副本.bin
End of files
/lfs1 closedir
csk6_rproc_init: node id: 2
Step1: Create rproc success!
/lfs1/cp_fw.bin stat: 0
Get file 'cp_fw.bin' size 79696
[Upgrade]read size 79696 write to 0x30000000
Load second core firmware success!
Step2: load second core firmware success!
----tester loop cnt =0---------
Step3: first jump config second core config success!
*** Booting Zephyr OS build c40252a4ad9e  ***
[Remote][main 30]loopcnt=0
[Remote][main 30]loopcnt=1
[Remote][main 30]loopcnt=2
[Remote][main 30]loopcnt=3
[Remote][main 30]loopcnt=4
[Remote][main 30]loopcnt=5
[Remote][main 30]loopcnt=6
[Remote][main 30]loopcnt=7
Step4: start second core success!
[Master]Ctrl remote core goto stop wait (1000) millisecond...
[Remote][main 30]loopcnt=8
[Master]Ctrl remote core goto stop wait (900) millisecond...
[Master]Ctrl remote core goto stop wait (800) millisecond...
[Remote][main 30]loopcnt=9
[Master]Ctrl remote core goto stop wait (700) millisecond...
[Remote][main 30]loopcnt=10
[Master]Ctrl remote core goto stop wait (600) millisecond...
[Remote][main 30]loopcnt=11
[Master]Ctrl remote core goto stop wait (500) millisecond...
[Remote][main [M3a0s]tleoro]pCctnrtl= 1r2e
mote core goto stop wait (400) millisecond...
[Master]Ctrl remote core goto stop wait (300) millisecond...
[Remote][main 30]loopcnt=13
[Master]Ctrl remote core goto stop wait (200) millisecond...
[Remote][main 30]loopcnt=14
[Master]Ctrl remote core goto stop wait (100) millisecond...
[Remote][main 30]loopcnt=15
Step5: stop second core success!
Step6: shutdown second core success!
----tester loop cnt =1---------
Step3: reconfig second core success!
*** Booting Zephyr OS build c40252a4ad9e  ***
[Remote][main 30]loopcnt=0
[Remote][main 30]loopcnt=1
[Remote][main 30]loopcnt=2
[Remote][main 30]loopcnt=3
[Remote][main 30]loopcnt=4
[Remote][main 30]loopcnt=5
[Remote][main 30]loopcnt=6
[Remote][main 30]loopcnt=7
Step4: start second core success!
[Master]Ctrl remote core goto stop wait (1000) millisecond...
[Remote][main 30]loopcnt=8
[Master]Ctrl remote core goto stop wait (900) millisecond...
[Master]Ctrl remote core goto stop wait (800) millisecond...
[Remote][main 30]loopcnt=9
[Master]Ctrl remote core goto stop wait (700) millisecond...
[Remote][main 30]loopcnt=10
[Master]Ctrl remote core goto stop wait (600) millisecond...
[Remote][main 30]loopcnt=11
[Master]Ctrl remote core goto stop wait (500) millisecond...
[Remote][mai[Mna s3t0e]rl]oCotprcln tr=e1m2o
te core goto stop wait (400) millisecond...
[Master]Ctrl remote core goto stop wait (300) millisecond...
[Remote][main 30]loopcnt=13
[Master]Ctrl remote core goto stop wait (200) millisecond...
[Remote][main 30]loopcnt=14
[Master]Ctrl remote core goto stop wait (100) millisecond...
[Remote][main 30]loopcnt=15
Step5: stop second core success!
Step6: shutdown second core success!
----tester loop cnt =2---------
Step3: reconfig second core success!
*** Booting Zephyr OS build c40252a4ad9e  ***
[Remote][main 30]loopcnt=0
[Remote][main 30]loopcnt=1
[Remote][main 30]loopcnt=2
[Remote][main 30]loopcnt=3
[Remote][main 30]loopcnt=4
[Remote][main 30]loopcnt=5
[Remote][main 30]loopcnt=6
[Remote][main 30]loopcnt=7
Step4: start second core success!
[Master]Ctrl remote core goto stop wait (1000) millisecond...
[Remote][main 30]loopcnt=8
[Master]Ctrl remote core goto stop wait (900) millisecond...
[Master]Ctrl remote core goto stop wait (800) millisecond...
[Remote][main 30]loopcnt=9
[Master]Ctrl remote core goto stop wait (700) millisecond...
[Remote][main 30]loopcnt=10
[Master]Ctrl remote core goto stop wait (600) millisecond...
[Remote][main 30]loopcnt=11
[Master]Ctrl remote core goto stop wait (500) millisecond...
[Remote][mai[Mna s3t0e]rl]oCotprcln tr=e1m2o
te core goto stop wait (400) millisecond...
[Master]Ctrl remote core goto stop wait (300) millisecond...
[Remote][main 30]loopcnt=13
[Master]Ctrl remote core goto stop wait (200) millisecond...
[Remote][main 30]loopcnt=14
[Master]Ctrl remote core goto stop wait (100) millisecond...
[Remote][main 30]loopcnt=15
Step5: stop second core success!
Step6: shutdown second core success
