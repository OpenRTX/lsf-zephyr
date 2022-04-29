
csk6 soc flash sample
####################

Overview
********

This sample demonstrates using the flash API on a SPI NOR serial flash
memory device.  While trivial it is an example of direct access and
allows confirmation that the flash is working.


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

   *** Booting Zephyr OS build b5a7185bc25f  ***

Litenai CSK6 Flash Testing
=========================

Test 1: Flash erase page at 0x7f8000
   Flash erase succeeded!

Test 2: Flash write (word array 1)
   Attempting to write 1122 at 0x7f8000
   Attempting to read 0x7f8000
   Data read: 1122
   Data read matches data written. Good!
   Attempting to write aabb at 0x7f8004
   Attempting to read 0x7f8004
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x7f8008
   Attempting to read 0x7f8008
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1234 at 0x7f800c
   Attempting to read 0x7f800c
   Data read: 1234
   Data read matches data written. Good!

Test 3: Flash erase (4 pages at 0x7f6000)
   Flash erase succeeded!

Test 4: Flash write (word array 2)
   Attempting to write 1234 at 0x7f8000
   Attempting to read 0x7f8000
   Data read: 1234
   Data read matches data written. Good!
   Attempting to write aabb at 0x7f8004
   Attempting to read 0x7f8004
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x7f8008
   Attempting to read 0x7f8008
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1122 at 0x7f800c
   Attempting to read 0x7f800c
   Data read: 1122
   Data read matches data written. Good!

Test 5: Flash erase page at 0x7f8000
   Flash erase succeeded!

Test 6: Non-word aligned write (word array 3)
   Attempting to write 1122 at 0x7f8001
   Attempting to read 0x7f8001
   Data read: 1122
   Data read matches data written. Good!
   Attempting to write aabb at 0x7f8005
   Attempting to read 0x7f8005
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x7f8009
   Attempting to read 0x7f8009
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1234 at 0x7f800d
   Attempting to read 0x7f800d
   Data read: 1234
   Data read matches data written. Good!
   Attempting to write 1122 at 0x7f8011
   Attempting to read 0x7f8011
   Data read: 1122
   Data read matches data written. Good!
   Attempting to write aabb at 0x7f8015
   Attempting to read 0x7f8015
   Data read: aabb
   Data read matches data written. Good!
   Attempting to write abcd at 0x7f8019
   Attempting to read 0x7f8019
   Data read: abcd
   Data read matches data written. Good!
   Attempting to write 1234 at 0x7f801d
   Attempting to read 0x7f801d
   Data read: 1234
   Data read matches data written. Good!

Test 7: Page layout API
   Offset  0x00041234:
     belongs to the page 65 of start offset 0x00041000
     and the size of 0x00001000 B.
   Page of number 37 has start offset 0x00025000
     and size of 0x00001000 B.
     Page index resolved properly
   SoC flash consists of 2048 pages.

Test 8: Write block size API
   write-block-size = 1


   
   

   
   

   
   
   
