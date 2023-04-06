
csk6 soc flash sample
####################

Overview
********

This sample demonstrates using the flash API on a SPI NOR serial flash
memory device.  While trivial it is an example of direct access and
allows confirmation that the flash is working.

FLash Unlock burner
********************
if you want to unlock the flash, you can use the flash_unlock_burner.bin
--burner option to burn the flash_unlock_burner.bin to the ram.

sudo cskburn -s /dev/ttyUSB0 -C 6 --burner flash_unlock_burner.bin -b 748800


Building and Running
********************

The application will build only for a target that has a :ref:`devicetree
<dt-guide>` entry with ``listenai,csk-nv-flash`` as a compatible.

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/driver/soc_flash_listenai
   :board: csk6
   :goals: build flash
   :compact:

Sample Output
=============

.. code-block:: console

   *** Booting Zephyr OS build v1.2.2-alpha.1-4-g3da07e8922ba  ***

Litenai CSK6 Flash Testing
=========================
Flash ID: 0x19400b
==main write flash==
mxic_program: Flash block locked. 
Flash write failed!
1Flash flash_lock succeeded!
Flash write succeeded!
Flash flash_unlock succeeded!

Test 1: Flash erase page at 0x760000
   Flash erase succeeded!

Test 2: Flash write (word array 1)
   Attempting to write 1122 at 0x760000
   Attempting to read 0x760000
   Data read: 1122
   Data read matches data written. Good!
   Attempting to write aabb at 0x760004
   Attempting to read 0x760004
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x760008
   Attempting to read 0x760008
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1234 at 0x76000c
   Attempting to read 0x76000c
   Data read: 1234
   Data read matches data written. Good!

Test 3: Flash erase (4 pages at 0x75e000)
   Flash erase succeeded!

Test 4: Flash write (word array 2)
   Attempting to write 1234 at 0x760000
   Attempting to read 0x760000
   Data read: 1234
   Data read matches data written. Good!
   Attempting to write aabb at 0x760004
   Attempting to read 0x760004
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x760008
   Attempting to read 0x760008
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1122 at 0x76000c
   Attempting to read 0x76000c
   Data read: 1122
   Data read matches data written. Good!

Test 5: Flash erase page at 0x760000
   Flash erase succeeded!

Test 6: Non-word aligned write (word array 3)
   Attempting to write 1122 at 0x760001
   Attempting to read 0x760001
   Data read: 1122
   Data read matches data written. Good!
   Attempting to write aabb at 0x760005
   Attempting to read 0x760005
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x760009
   Attempting to read 0x760009
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1234 at 0x76000d
   Attempting to read 0x76000d
   Data read: 1234
   Data read matches data written. Good!
   Attempting to write 1122 at 0x760011
   Attempting to read 0x760011
   Data read: 1122
   Data read matches data written. Good!
   Attempting to write aabb at 0x760015
   Attempting to read 0x760015
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x760019
   Attempting to read 0x760019
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1234 at 0x76001d
   Attempting to read 0x76001d
   Data read: 1234
   Data read matches data written. Good!

Test 7: Page layout API
   Offset  0x00041234:
     belongs to the page 65 of start offset 0x00041000
     and the size of 0x00001000 B.
   Page of number 37 has start offset 0x00025000
     and size of 0x00001000 B.
     Page index resolved properly
   SoC flash consists of 4096 pages.

Test 8: Write block size API
   write-block-size = 4

Test 9: Write data form internal flash address area
   Flash erase succeeded!
   Attempting to read 0x760000
   Data read matches data written. Good!


   
   

   
   

   
   
   
