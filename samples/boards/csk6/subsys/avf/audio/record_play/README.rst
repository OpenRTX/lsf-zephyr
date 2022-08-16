acapture Sample Application
##########################

Overview
********

This sample demonstrates how to use acapture & aplay(provided by AVF Framework) to 
startup a audio recoder and audio player. DSP firmware and SOF topology file will 
be invoked in byte-array format by generate_inc_file_for_target() camke function 
which provided by Zephyr Build System.

In this sample project, includes the firmware and topology file resources required 
for DSP operation (supporrt PCM 16bits 16000 format).Developers can replace files 
according to project requirements.
Hardware info:
MIC0_N/MIC0_P  ->   Audio input
GIPOA04  ->  Power amplifier enable
LINE_LN  ->  Audio output
LINE_LP  ->  Audio output


Building
***********************************************

The sample can be built as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/avf/audio/acapture
   :board: csk6002_9s_nano
   :goals: build flash
   :compact:


Sample Output
***********************************************

*** Booting Zephyr OS build v1.0.4-alpha.1  ***
Create acapture success
Register data event callback success.
Set audio fmt<16000 1 16> to acapture success
Trigger capture start success.
record start
Create aplay success
Set audio fmt to aplay success
Trigger play start success.
play drain finsh with timestamp 10640 ms
audio play complete. 