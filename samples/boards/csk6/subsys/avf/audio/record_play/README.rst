acapture Sample Application
##########################

Overview
********

This sample shows how to use the API interface provided by acapture to record audio.
And also shows how to use the API interface provided by Aplay to play audio. 

In this sample project, includes the firmware and topology file resources required 
for DSP operation (only supporrt PCM 16bits 16000 format).Developers can replace files 
according to project requirements.
Hardware info:
MIC0_N/MIC0_P  ->   Audio input

includes the firmware and topology file resources required 
for DSP operation (only supporrt PCM 16bits 48000 format).Developers can replace files 
according to project requirements.
Hardware info:
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

*** Booting Zephyr OS build 1aac66ee60b1  ***
Create acapture success
Register data event callback success.
Set audio fmt<16000 1 16> to acapture success
Trigger capture start success.
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
Get audio data length 4096 
acapture run compelete and exit
