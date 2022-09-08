sqlite3_littefs Sample Application
##########################

Overview
********

This sample will show how to use the related API interface of sqlite3_littefs. 
This example shows the operation commands of the sqlite3 on the littefs file system


Building
***********************************************

The sample can be built as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/sqlite3_littefs
   :board: csk6012_c3_nano
   :goals: build flash
   :compact:


Sample Output
***********************************************

*** Booting Zephyr OS build v1.0.4-alpha.1-8862-g9133a30a9293  ***
/lfs1 automounted
fs_unlink: rc = 0
fs_unlink: rc = 0
fs_open = 0x861d8
Opened database successfully
fs_open = 0x87f28
Opened database successfully
CREATE TABLE test1 (id INTEGER, content);
Operation done successfully
Time taken: 311551 us
CREATE TABLE test2 (id INTEGER, content);
Operation done successfully
Time taken: 183003 us
INSERT INTO test1 VALUES (1, 'Hello, World from test1');
Operation done successfully
Time taken: 111396 us
INSERT INTO test1 VALUES (2, 'over, World from test1');
Operation done successfully
Time taken: 184210 us
INSERT INTO test2 VALUES (1, 'Hello, World from test2');
Operation done successfully
Time taken: 114266 us
SELECT * FROM test1
Callback function called: id = 1
content = Hello, World from test1

Callback function called: id = 2
content = over, World from test1

Operation done successfully
Time taken: 19026 us
SELECT * FROM test2
Callback function called: id = 1
content = Hello, World from test2

Operation done successfully
Time taken: 12365 us
/lfs1 unmount: 0
CSK sqlite3 test complete!
[00:00:00.000,000] <inf> littlefs: littlefs partition at /lfs1
[00:00:00.000,000] <inf> littlefs: LittleFS version 2.4, disk version 2.0
[00:00:00.000,000] <inf> littlefs: FS at FLASH_CTRL:0x160000 is 768 0x1000-byte blocks with 512 cycle
[00:00:00.000,000] <inf> littlefs: sizes: rd 16 ; pr 16 ; ca 64 ; la 32
[00:00:00.003,000] <inf> littlefs: /lfs1 mounted
[00:00:00.003,000] <inf> littlefs: Automount /lfs1 succeeded
[00:00:01.015,000] <inf> littlefs: /lfs1 unmounted