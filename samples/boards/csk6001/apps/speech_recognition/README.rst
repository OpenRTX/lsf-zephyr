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
   :zephyr-app: samples/boards/csk6001/apps/speech_recognition
   :board: csk6002_c3_nano
   :goals: debug

Open a serial terminal (minicom, putty, etc.) and connect the board with the
following settings:

- Speed: 115200
- Data: 8 bits
- Parity: None
- Stop bits: 1

AP console
**********

.. code-block:: console

*** Booting Zephyr OS build 711f05726bfa  ***
metal: debug:     registered generic bus
/NAND: mount: 0
/NAND:: bsize = 512 ; frsize = 2048 ; blocks = 3058 ; bfree = 977
/NAND: opendir: 0
  D 0 SYSTEM~1
  F 4255028 CP_FW.BIN
End of files
/NAND: closedir
csk6001_rproc_init: node id: 2
Step1: Create rproc success!
/NAND:/zephyr.bin stat: 0
Get file 'CP_FW.BIN' size 4255028
[Upgrade]read size 4255028 write to 0x30000000
Load second core firmware success!
Step2: load second core firmware success!
Step3: first jump config second core config success!
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


CP console
**********

.. code-block:: console

*** Booting Zephyr OS build 711f05726bfa  ***
[Remote][main 62] Enter...
[Remote][sys_init 57] Done...
json: {"wrap":{"version":"4002.aiwrap.v1000.2.0.4 beta (Sep 30 2021 10:42:28)","inst_size":1672,"samples":{"in":256,"out":2560}},"cae":{"version":"6001.cae.v1009.3.2.0 beta (Sep 30 2021 11:49:39)","memory":{"inst_ram_size":65024,"temp_ram_size":590336,"psram_size":2094816},"feature":{"mic_num":2,"ref_num":2,"local_out_channels":3,"asr_out_channels":1,"nbit":16,"nsample":16000,"frame_num":10,"frame_size":256,"support_pre_wk_info":1,"support_wk_info":1}},"esr":{"version":"6001.miniesr.v1266.1.0.0 beta (Jul 27 2021 18:54:45)","memory":{"share_mem_size":194304,"inst_mem_size":442016},"feature":{"input_pcm_channel":3,"input_pcm_bit":16,"input_pcm_sample_rate":16000,"max_main_word_count":5,"max_asr_word_count":250}}}
memory used: psram=0x26bc20(%0.3fKB), share=0xa0000(%0.3fKB)
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1702, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1401, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1748, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1418, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1689, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1262, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1673, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1410, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1642, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1676, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1670, [Kid]: 0 
WAKEUP:[KEY]:xiao3 you1 xiao3 you1, [NCM]:1718, [Kid]: 0 
