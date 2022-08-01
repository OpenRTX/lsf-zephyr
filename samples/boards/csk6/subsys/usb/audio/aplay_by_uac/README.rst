.. _usb_audio_headset:

Aplay by USB Audio Sample Application
############################

Overview
********

This sample app demonstrates how to use aplay to play usb audio. 

Building and Running
********************

In order to build the sample an overlay file with required options
must be provided. By some default overlay are added in board folder. An overlay contains
software and hardware specific information which allow to fully
describe the device.

After you have built and flashed the sample app image to your board, plug the
board into a host device.

Testing
*******

Steps to test the sample:
- Build and flash the sample as described above.
- Connect to the HOST.
- Chose default Audio IN/OUT.
- Start streaming audio (for example by playing an audio file on the HOST).
- Start recording audio stream (for example using Audacity).
- Verify the recorded audio stream.
