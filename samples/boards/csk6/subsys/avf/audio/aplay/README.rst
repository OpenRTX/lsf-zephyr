aplay Sample Application
##########################

Overview
********

This sample shows how to use the API interface provided by Aplay to play audio. 
In this sample project, includes the firmware and topology file resources required 
for DSP operation (only supporrt PCM 16bits 48000 format).Developers can replace files 
according to project requirements.
Hardware info:
GIPOA04  ->  Power amplifier enable
LINE_LN  ->  Audio output


Building
***********************************************

The sample can be built as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/avf/audio/aplay
   :board: csk6002_9s_nano
   :goals: build flash
   :compact:


Sample Output
***********************************************

*** Booting Zephyr OS build 30291d12373f  ***
Prepare start play wav and get audio fmt < PCM,sample rate 48000 channels 1 bits 16 >
Create aplay success
Set audio fmt to aplay success
Trigger play start success.
Write play audio data compelete total size 303266 with timestamp 3417 ms
play drain finsh with timestamp 3707 ms
aplay run compelete and exit.


