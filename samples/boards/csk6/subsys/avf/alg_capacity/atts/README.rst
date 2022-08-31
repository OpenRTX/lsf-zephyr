atts Sample Application
##########################

Overview
********

Hardware info:
GIPOA04  ->  Power amplifier enable
LINE_LP/LINE_LN  ->  Audio output


Building
***********************************************

The sample can be built as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/avf/alg_capacity/atts
   :board: csk6002_9s_nano
   :goals: build flash
   :compact:


Sample Output
***********************************************

*** Booting Zephyr OS build v1.0.2-beta.1-108-geb6a63b8e7cb  ***
avf platform register success
tts playback start
atts play over!
atts play success!
tts playback over



