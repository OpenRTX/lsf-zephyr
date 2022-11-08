csk_malloc Sample Application
##########################

Overview
********

This sample will show how to use the related API interface of csk_malloc. 
The API provides a methods similar to k_malloc, but the heap's memory address can be placed in 
user-defined memory segments.


Building
***********************************************

The sample can be built as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/csk_malloc
   :board: csk6002_9s_nano
   :goals: build flash
   :compact:


Sample Output
***********************************************

*** Booting Zephyr OS build v1.0.3-alpha.1  ***

Test 1: ################csk_malloc################
System init csk heap info:
Heap at 0x30600000 contains 511 units in 9 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------
        8          256            1          504         4028

4028 free bytes, 0 allocated bytes, overhead = 64 bytes (1.6%)
Malloc success ptr 0x30600040
After malloc 1000 bytes ,heap info:
Heap at 0x30600000 contains 511 units in 9 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------
        8          256            1          378         3020

3020 free bytes, 1004 allocated bytes, overhead = 68 bytes (1.7%)

Test 2: ################csk_realloc################
csk_realloc success ptr 0x30600044
After realloc 4000 bytes ,heap info:
Heap at 0x30600000 contains 511 units in 9 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------
        1            2            1            2           12

12 free bytes, 4012 allocated bytes, overhead = 68 bytes (1.7%)

Test 3: ################csk_calloc################
csk_calloc success ptr_calloc 0x30600ff0,data[0...7]={0,0,0,0,0,0,0,0}
After csk_calloc numb 2 size 4 bytes ,heap info:
Heap at 0x30600000 contains 511 units in 9 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------

0 free bytes, 4024 allocated bytes, overhead = 68 bytes (1.7%)

Test 4: ################csk_free################
After free all ,heap info:
Heap at 0x30600000 contains 511 units in 9 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------
        8          256            1          504         4028

4028 free bytes, 0 allocated bytes, overhead = 64 bytes (1.6%)
CSK Heap test complete!
