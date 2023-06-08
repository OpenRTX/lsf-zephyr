.. _display-sample:

Display Sample
##############

Overview
********

This sample will draw some basic rectangles onto the display.
The rectangle colors and positions are chosen so that you can check the
orientation of the LCD and correct RGB bit order. The rectangles are drawn
in clockwise order, from top left corner: red, green, blue, grey. The shade of
grey changes from black through to white. If the grey looks too green or red
at any point or the order of the corners is not as described above then the LCD
may be endian swapped.

Building and Running
********
