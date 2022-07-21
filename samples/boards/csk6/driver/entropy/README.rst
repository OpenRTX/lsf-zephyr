Overview
********
    This sample is used to demonstrate how to use entropy driver.

Building and Running
********************
    Make sure you have the csk6 boards connected over Jlink debuger.

    The sample can be built and flashed as follows:

.. code-block:: console

   lisa zep build -b csk6002_9s_nano
   lisa zep flash -r jlink

Sample Output
=============

.. code-block:: console

    *** Booting Zephyr OS build v1.0.3-alpha.2-1-gdb9508e43c43  ***
    start get entropy sample 
    entropy buf dump: 
    11  cc  43  d1  02  42  ff  92  a6  78  dc  e9  33  8e  89  24  
    85  a4  27  3f  a2  84  00  eb  9f  5d  22  1f  6b  ad  83  8b  
    7e  37  82  f7  d8  94  28  b3  74  f4  1d  c0  75  6a  eb  f7  
    9a  89  a6  c5  7f  0a  ce  d9  98  45  b0  09  b7  43  65  2d  
    7e  50  72  3a  15  00  5e  2f  64  f5  aa  f5  2e  c2  85  0d  
    14  18  ad  d2  85  41  9d  5a  93  65  14  55  8c  34  2d  6a  
    fc  b7  a5  76  d0  6d  09  13  f3  76  e3  3b  c9  21  d5  08  
    a3  64  61  7c  cc  c3  5e  e3  3a  65  9f  07  5b  b6  08  5c  
    22  5c  ec  80  74  0f  05  12  7a  09  ce  7d  6f  a5  79  71  
    ac  20  27  be  75  ad  a6  95  59  20  3a  a2  6d  da  9c  a7  
    3f  01  bb  6f  3d  a6  ed  7d  21  22  6a  d1  51  f8  44  d3  
    85  d4  b6  b5  29  89  14  42  b8  d4  54  76  26  f7  d7  61  
    60  f4  c8  3e  6a  85  51  2a  f0  f5  fe  c0  03  95  6f  9e  
    14  19  62  c0  a7  ee  cb  5c  c6  1d  11  b1  97  45  8f  8c  
    da  92  d6  82  ee  b0  57  66  fb  54  02  3c  df  88  e6  80  
    2a  52  01  25  bc  15  64  cf  10  e6  8a  66  e3  01  0b  73  

    entropy sample done