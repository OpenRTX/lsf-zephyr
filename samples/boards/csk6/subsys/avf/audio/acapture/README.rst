acapture Sample Application
##########################

Overview
********

This sample shows how to use the API interface provided by acapture to record audio.
In this sample project, includes the firmware and topology file resources required
for DSP operation (only supporrt PCM 16bits 16000 format).Developers can replace files
according to project requirements.
Hardware info:
MIC0_N/MIC0_P  ->   Audio input


Building
***********************************************

The sample can be built as follows:

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/avf/audio/acapture
   :board: csk6011a_nano
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
