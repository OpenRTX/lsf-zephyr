Sof_host Sample Application
##########################

Overview
********

This application mainly shows the SOF process and related configuration.
Before running this example, you first need to package the second core firmware and tplg file(default see sample/resources/* ) 
into the file system and burn it to the corresponding Flash area address.

Prepare 
***********************************************
- Build example project
- burn the zephyr.bin
- burn file system image
  - if use fatfs
    -After burning the firmware, insert the USB to map the USB disk on the computer and copy the contents of the ./resource folder to the USB disk
  - if use littlefs
    -Burn the file system image use "lisa zep fs:build" and  "lisa zep flash:fs"

Building the application for csk6002_c3_nano
***********************************************

.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/subsys/sof_host
   :board: csk6002_c3_nano
   :goals: debug

Open a serial terminal (minicom, putty, etc.) and connect the board with the
following settings:

- Speed: 115200
- Data: 8 bits
- Parity: None
- Stop bits: 1

.. code-block:: console

*** Booting Zephyr OS build 2ed5423c6d10  ***
/NAND: mount: 0
/NAND:: bsize = 512 ; frsize = 1024 ; blocks = 6112 ; bfree = 4873
/NAND: opendir: 0
  D 0 System Volume Information
  F 184320 vcapture.img
  F 163840 acapture111.pcm
  D 0 tplg
  F 24281 earthquake_16k_16bit_single.mp3
  F 320300 earthquake_16k_16bit_single.wav
  F 530896 zephyr.bin
  F 12447 csk6_default.tplg
End of files
/NAND: closedir
[sof]"IPM initialized\n"
[sof]"[IMPLEMENTATION MISSING : dev_set_name]\n"
[sof]"ASoC: binding sai3-wm8960-hifi\n"
[sof]"[IMPLEMENTATION MISSING : dev_set_name]\n"
[sof]"[IMPLEMENTATION MISSING : device_register]\n"
[sof]"ASoC: binding sai1-wm8960-hifi\n"
[sof]"[IMPLEMENTATION MISSING : dev_set_name]\n"
[sof]"[IMPLEMENTATION MISSING : device_register]\n"
[sof]"[IMPLEMENTATION MISSING : devm_ioremap]\n"
[sof]"[IMPLEMENTATION MISSING : devm_ioremap]\n"
[sof]"[IMPLEMENTATION MISSING : csk6_parse_clocks]\n"
[sof]"[IMPLEMENTATION MISSING : csk6_enable_clocks]\n"
[sof]"fw_state change: 0 -> 1\n"
[sof]"Force to use nocodec mode\n"
[sof]"Using nocodec machine driver\n"
[sof]"loading firmware\n"
[sof]first
[sof]"/NAND:/zephyr.bin stat: 0\n"
[sof]"Get file 'zephyr.bin' size 530896\n"
[sof]"[csk6_request_firmware 651]get firmware %S size(660344) success!\n"
[sof]"fw_state change: 1 -> 2\n"
[sof]"[IMPLEMENTATION MISSING : debugfs_create_file]\n"
[sof]"booting DSP firmware\n"


[00:00:00.006,000] <inf> usb_msc: Sect Count 12288
[00:00:00.006,000] <inf> usb_msc: Memory Size 6291456
uart:~$ [sof]"ipc rx: 0x70000000: FW_READY\n"
[sof]"ipc: DSP is ready 0x70000000 offset 0x0\n"
[sof]"Firmware info: version 1:8:0-6e784\n"
[sof]"Firmware: ABI 3:19:0 Kernel ABI 3:18:0\n"
[sof]"warn: FW ABI is more recent than kernel\n"
[sof]"error: have no window info\n"
[sof]"fw_state change: 2 -> 5\n"
[sof]"ipc rx done: 0x70000000: FW_READY\n"
[sof]"firmware boot complete\n"
[sof]"fw_state change: 5 -> 6\n"
[sof]"SOF firmware trace disabled\n"
[sof]"[IMPLEMENTATION MISSING : devm_snd_soc_register_component]\n"
[sof]" mailbox upstream 0x0 - size 0x200\n"
[sof]" mailbox downstream 0x200 - size 0x200\n"
[sof]" stream region 0x1400 - size 0x200\n"
[sof]" debug region 0x400 - size 0x800\n"
[sof]"loading topology:/NAND:/csk6_default.tplg\n"
[sof]"/NAND:/csk6_default.tplg stat: 0\n"
[sof]"Get file 'csk6_default.tplg' size 12447\n"
[sof]"[request_firmware 72]get firmware size(12447) success!\n"
[sof]"ASoC: Got 0x73 bytes of type 8 version 0 vendor 0 at pass 0\n"
[sof]"Topology: ABI 3:19:0 Kernel ABI 3:18:0\n"
[sof]"warn: topology ABI is more recent than kernel\n"
[sof]"ASoC: Got 0x790 bytes of type 5 version 0 vendor 0 at pass 3\n"
[sof]"ASoC: adding 6 DAPM widgets\n"
[sof]"ASoC: creating DAPM widget PCM0P id 11\n"
[sof]"tplg: ready widget id 0 pipe 1 type 23 name : PCM0P stream Passthrough Playback 0\n"
[sof]"loaded host PCM0P\n"
[sof]" config: periods snk 4 src 0 fmt 0\n"
[sof]"widget add to list <PCM0P> \n"
[sof]"ASoC: creating DAPM widget CODEC_ADAPTER1.0 id 18\n"
[sof]"ASoC: adding bytes kcontrol CA Setup Config1 with access 0x10000030\n"
[sof]"tplg: load control type 2 name : CA Setup Config1\n"
[sof]"tplg: load kcontrol index 1 chans 0\n"
[sof]"ASoC: adding bytes kcontrol CA Runtime Params1 with access 0x10000030\n"
[sof]"tplg: load control type 2 name : CA Runtime Params1\n"
[sof]"tplg: load kcontrol index 1 chans 0\n"
[sof]"ASoC: template CODEC_ADAPTER1.0 with 0/0/2 (mixer/enum/bytes) control\n"
[sof]"tplg: ready widget id 1 pipe 1 type 33 name : CODEC_ADAPTER1.0 stream none\n"
[sof]"[sof_get_control_data 2054][tplg]num_kcontrols 2 kcontrol_type 2\n"
[sof]"[sof_get_control_data 2054][tplg]num_kcontrols 2 kcontrol_type 2\n"
[sof]" config: periods snk 4 src 4 fmt 2\n"
[sof]"widget add to list <CODEC_ADAPTER1.0> \n"
[sof]"ASoC: creating DAPM widget BUF1.0 id 16\n"
[sof]"tplg: ready widget id 2 pipe 1 type 31 name : BUF1.0 stream none\n"
[sof]"buffer BUF1.0: size 4096 caps 0x61\n"
[sof]"widget add to list <BUF1.0> \n"
[sof]"ASoC: creating DAPM widget BUF1.1 id 16\n"
[sof]"tplg: ready widget id 3 pipe 1 type 31 name : BUF1.1 stream none\n"
[sof]"buffer BUF1.1: size 4096 caps 0x61\n"
[sof]"widget add to list <BUF1.1> \n"
[sof]"ASoC: creating DAPM widget SAI3.OUT id 13\n"
[sof]"tplg: ready widget id 4 pipe 1 type 27 name : SAI3.OUT stream sai3-wm8960-hifi\n"
[sof]"dai SAI3.OUT: type 5 index 3\n"
[sof]" config: periods snk 0 src 2 fmt 2\n"
[sof]"tplg: check widget: SAI3.OUT stream: sai3-wm8960-hifi dai stream: sai3-wm8960-hifi\n"
[sof]"tplg: connected widget SAI3.OUT -> DAI link sai3-wm8960-hifi\n"
[sof]"tplg: check widget: SAI3.OUT stream: sai3-wm8960-hifi dai stream: sai1-wm8960-hifi\n"
[sof]"widget add to list <SAI3.OUT> \n"
[sof]"ASoC: creating DAPM widget PIPELINE.1.SAI3.OUT id 17\n"
[sof]"tplg: ready widget id 5 pipe 1 type 32 name : PIPELINE.1.SAI3.OUT stream SAI3.OUT\n"
[sof]"tplg: pipeline id 1 comp 5 scheduling comp id 4\n"
[sof]"pipeline PIPELINE.1.SAI3.OUT: period 1000 pri 0 mips 5000 core 0 frames 0 dynamic 0\n"
[sof]"widget add to list <PIPELINE.1.SAI3.OUT> \n"
[sof]"ASoC: Got 0x6c0 bytes of type 5 version 0 vendor 0 at pass 3\n"
[sof]"ASoC: adding 6 DAPM widgets\n"
[sof]"ASoC: creating DAPM widget PCM0C id 12\n"
[sof]"tplg: ready widget id 6 pipe 2 type 24 name : PCM0C stream Passthrough Capture 0\n"
[sof]"loaded host PCM0C\n"
[sof]" config: periods snk 0 src 2 fmt 0\n"
[sof]"widget add to list <PCM0C> \n"
[sof]"ASoC: creating DAPM widget PGA2.0 id 4\n"
[sof]" adding DAPM widget mixer control 2 Master Capture Volume\n"
[sof]"tplg: load control type 1 name : 2 Master Capture Volume\n"
[sof]"tplg: load kcontrol index 7 chans 2\n"
[sof]"[IMPLEMENTATION MISSING : strscpy]\n"
[sof]"ASoC: template PGA2.0 with 1/0/0 (mixer/enum/bytes) control\n"
[sof]"tplg: ready widget id 7 pipe 2 type 6 name : PGA2.0 stream none\n"
[sof]" config: periods snk 2 src 4 fmt 2\n"
[sof]"widget add to list <PGA2.0> \n"
[sof]"ASoC: creating DAPM widget BUF2.0 id 16\n"
[sof]"tplg: ready widget id 8 pipe 2 type 31 name : BUF2.0 stream none\n"
[sof]"buffer BUF2.0: size 4096 caps 0x61\n"
[sof]"widget add to list <BUF2.0> \n"
[sof]"ASoC: creating DAPM widget BUF2.1 id 16\n"
[sof]"tplg: ready widget id 9 pipe 2 type 31 name : BUF2.1 stream none\n"
[sof]"buffer BUF2.1: size 4096 caps 0x61\n"
[sof]"widget add to list <BUF2.1> \n"
[sof]"ASoC: creating DAPM widget SAI3.IN id 14\n"
[sof]"tplg: ready widget id 10 pipe 2 type 28 name : SAI3.IN stream sai3-wm8960-hifi\n"
[sof]"dai SAI3.IN: type 5 index 3\n"
[sof]" config: periods snk 2 src 0 fmt 2\n"
[sof]"tplg: check widget: SAI3.IN stream: sai3-wm8960-hifi dai stream: sai3-wm8960-hifi\n"
[sof]"tplg: connected widget SAI3.IN -> DAI link sai3-wm8960-hifi\n"
[sof]"tplg: check widget: SAI3.IN stream: sai3-wm8960-hifi dai stream: sai1-wm8960-hifi\n"
[sof]"widget add to list <SAI3.IN> \n"
[sof]"ASoC: creating DAPM widget PIPELINE.2.SAI3.IN id 17\n"
[sof]"tplg: ready widget id 11 pipe 2 type 32 name : PIPELINE.2.SAI3.IN stream SAI3.IN\n"
[sof]"tplg: pipeline id 2 comp 11 scheduling comp id 10\n"
[sof]"pipeline PIPELINE.2.SAI3.IN: period 1000 pri 0 mips 5000 core 0 frames 0 dynamic 0\n"
[sof]"widget add to list <PIPELINE.2.SAI3.IN> \n"
[sof]"ASoC: Got 0x548 bytes of type 5 version 0 vendor 0 at pass 3\n"
[sof]"ASoC: adding 6 DAPM widgets\n"
[sof]"ASoC: creating DAPM widget PCM3C id 12\n"
[sof]"tplg: ready widget id 12 pipe 3 type 24 name : PCM3C stream SRC Capture 3\n"
[sof]"loaded host PCM3C\n"
[sof]" config: periods snk 0 src 3 fmt 0\n"
[sof]"widget add to list <PCM3C> \n"
[sof]"ASoC: creating DAPM widget SRC3.0 id 20\n"
[sof]"tplg: ready widget id 13 pipe 3 type 34 name : SRC3.0 stream none\n"
[sof]"src SRC3.0: source rate 48000 sink rate 0\n"
[sof]" config: periods snk 4 src 3 fmt 2\n"
[sof]"widget add to list <SRC3.0> \n"
[sof]"ASoC: creating DAPM widget BUF3.0 id 16\n"
[sof]"tplg: ready widget id 14 pipe 3 type 31 name : BUF3.0 stream none\n"
[sof]"buffer BUF3.0: size 4096 caps 0x61\n"
[sof]"widget add to list <BUF3.0> \n"
[sof]"ASoC: creating DAPM widget BUF3.1 id 16\n"
[sof]"tplg: ready widget id 15 pipe 3 type 31 name : BUF3.1 stream none\n"
[sof]"buffer BUF3.1: size 4096 caps 0x61\n"
[sof]"widget add to list <BUF3.1> \n"
[sof]"ASoC: creating DAPM widget SSP0.IN id 14\n"
[sof]"tplg: ready widget id 16 pipe 3 type 28 name : SSP0.IN stream sai1-wm8960-hifi\n"
[sof]"dai SSP0.IN: type 1 index 0\n"
[sof]" config: periods snk 2 src 0 fmt 2\n"
[sof]"tplg: check widget: SSP0.IN stream: sai1-wm8960-hifi dai stream: sai3-wm8960-hifi\n"
[sof]"tplg: check widget: SSP0.IN stream: sai1-wm8960-hifi dai stream: sai1-wm8960-hifi\n"
[sof]"tplg: connected widget SSP0.IN -> DAI link sai1-wm8960-hifi\n"
[sof]"widget add to list <SSP0.IN> \n"
[sof]"ASoC: creating DAPM widget PIPELINE.3.SSP0.IN id 17\n"
[sof]"tplg: ready widget id 17 pipe 3 type 32 name : PIPELINE.3.SSP0.IN stream SSP0.IN\n"
[sof]"tplg: pipeline id 3 comp 17 scheduling comp id 16\n"
[sof]"pipeline PIPELINE.3.SSP0.IN: period 1000 pri 0 mips 5000 core 0 frames 0 dynamic 0\n"
[sof]"widget add to list <PIPELINE.3.SSP0.IN> \n"
[sof]"ASoC: Got 0x720 bytes of type 7 version 0 vendor 0 at pass 4\n"
[sof]"tplg: load pcm Port0 0\n"
[sof]"tplg: pcm Port0 stream tokens: playback d0i3:0\n"
Heap at 0x30704000 contains 63486 units in 16 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------
        8          257            1          501         4000
       15        32769            1        62462       499688

503688 free bytes, 4096 allocated bytes, overhead = 112 bytes (0.0%)
[sof]"tplg: pcm Port0 stream tokens: capture d0i3:0\n"
Heap at 0x30704000 contains 63486 units in 16 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------
        8          257            2          511         4080
       15        32769            1        61438       491496

499576 free bytes, 8192 allocated bytes, overhead = 128 bytes (0.0%)
[dev_name 1017]The name is null!
[sof]"ASoC: dynamically register DAI \n"
[sof]"ASoC: Registered DAI 'Port0 0'\n"
[sof]"[sof_link_load 3242]link->id 0 link->name Port0,link->stream_name Port0 link->platforms 0xa77c4 link->no_pcm 0!\n"
[dev_name 1017]The name is null!
[sof]"ASoC: binding Port0\n"
[sof]"[IMPLEMENTATION MISSING : dev_set_name]\n"
[sof]"[IMPLEMENTATION MISSING : device_register]\n"
[sof]"tplg: load pcm Port1 3\n"
[sof]"tplg: pcm Port1 stream tokens: capture d0i3:0\n"
Heap at 0x30704000 contains 63486 units in 16 buckets

  bucket#    min units        total      largest      largest
             threshold       chunks      (units)      (bytes)
  -----------------------------------------------------------
        8          257            3          511         4080
       15        32769            1        60414       483304

495464 free bytes, 12288 allocated bytes, overhead = 144 bytes (0.0%)
[dev_name 1017]The name is null!
[sof]"ASoC: dynamically register DAI \n"
[sof]"ASoC: Registered DAI 'Port1 3'\n"
[sof]"[sof_link_load 3242]link->id 3 link->name Port1,link->stream_name Port1 link->platforms 0xa8ba4 link->no_pcm 0!\n"
[dev_name 1017]The name is null!
[sof]"ASoC: binding Port1\n"
[sof]"[IMPLEMENTATION MISSING : dev_set_name]\n"
[sof]"[IMPLEMENTATION MISSING : device_register]\n"
[sof]"ASoC: adding 2 PCM DAIs\n"
[sof]"ASoC: Got 0x210 bytes of type 4 version 0 vendor 0 at pass 5\n"
[sof]"ASoC: adding 4 DAPM routes for index 1\n"
[sof]"sink BUF1.0 control none source PCM0P\n"
[sof]"sink CODEC_ADAPTER1.0 control none source BUF1.0\n"
[sof]"sink BUF1.1 control none source CODEC_ADAPTER1.0\n"
[sof]"sink SAI3.OUT control none source BUF1.1\n"
[sof]"ASoC: Got 0x210 bytes of type 4 version 0 vendor 0 at pass 5\n"
[sof]"ASoC: adding 4 DAPM routes for index 2\n"
[sof]"sink PCM0C control none source BUF2.0\n"
[sof]"sink BUF2.0 control none source PGA2.0\n"
[sof]"sink PGA2.0 control none source BUF2.1\n"
[sof]"sink BUF2.1 control none source SAI3.IN\n"
[sof]"ASoC: Got 0x210 bytes of type 4 version 0 vendor 0 at pass 5\n"
[sof]"ASoC: adding 4 DAPM routes for index 3\n"
[sof]"sink PCM3C control none source BUF3.0\n"
[sof]"sink BUF3.0 control none source SRC3.0\n"
[sof]"sink SRC3.0 control none source BUF3.1\n"
[sof]"sink BUF3.1 control none source SSP0.IN\n"
[sof]"ASoC: Got 0xe00 bytes of type 10 version 0 vendor 0 at pass 8\n"
[sof]"[sof_link_load 3242]link->id 0 link->name sai3-wm8960-hifi,link->stream_name sai3-wm8960-hifi link->platforms 0x9fc9c link->no_pcm 1!\n"
[dev_name 1017]The name is null!
[sof]"tplg: 1 hw_configs found, default id: 0!\n"
[sof]"[sof_link_load 3332]config.type 5\n"
[sof]"tplg: config SAI3 fmt 0x1 mclk 12288000 width 32 slots 2 mclk id 0\n"
[sof]"set DAI config for sai3-wm8960-hifi index 3\n"
[sof]"set DAI config for sai3-wm8960-hifi index 3\n"
[sof]"[sof_link_load 3242]link->id 1 link->name sai1-wm8960-hifi,link->stream_name sai1-wm8960-hifi link->platforms 0xa05e4 link->no_pcm 1!\n"
[dev_name 1017]The name is null!
[sof]"tplg: 1 hw_configs found, default id: 1!\n"
[sof]"[sof_link_load 3332]config.type 1\n"
[sof]"tplg: config SSP0 fmt 0x4001 mclk 19200000 bclk 1920000 fclk 48000 width (16)20 slots 2 mclk id 0 quirks 0 clks_control 0x0\n"
[sof]"set DAI config for sai1-wm8960-hifi index 0\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	5c 00 00 00 00 00 01 30 00 00 00 00 01 00 00 00 
	01 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 04 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 0c 10 9d 8b 
	78 6d 8f 41 90 a3 e0 e8 05 d0 85 2b ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"widget PCM0P setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	80 00 00 00 00 00 01 30 01 00 00 00 00 00 00 00 
	01 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 04 00 00 00 04 00 00 00 00 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	0c 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 0c 00 00 00 10 00 00 00 
	43 84 21 d8 f3 5f 4c 4a b3 88 6c fe 07 b9 56 aa ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	7c 00 00 00 00 00 03 50 00 00 00 00 01 00 00 00 
	05 00 00 00 03 00 00 00 01 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 53 4f 46 00 
	01 00 00 00 00 00 00 00 00 10 00 03 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x50030000: GLB_COMP_MSG: SET_DATA\n"
[sof]"ipc tx succeeded: 0x50030000: GLB_COMP_MSG: SET_DATA\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	7c 00 00 00 00 00 04 50 00 00 00 00 01 00 00 00 
	04 00 00 00 03 00 00 00 01 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 53 4f 46 00 
	01 00 00 00 00 00 00 00 00 10 00 03 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x50040000: GLB_COMP_MSG: GET_DATA\n"
[sof]"ipc tx succeeded: 0x50040000: GLB_COMP_MSG: GET_DATA\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	88 00 00 00 00 00 03 50 00 00 00 00 01 00 00 00 
	05 00 00 00 03 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 0c 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 53 4f 46 00 
	00 00 00 00 0c 00 00 00 00 10 00 03 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	0c 00 00 00 10 00 00 00 ]
[sof]"ipc tx: 0x50030000: GLB_COMP_MSG: SET_DATA\n"
[sof]"ipc tx succeeded: 0x50030000: GLB_COMP_MSG: SET_DATA\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	88 00 00 00 00 00 04 50 00 00 00 00 01 00 00 00 
	04 00 00 00 03 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 0c 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 53 4f 46 00 
	00 00 00 00 0c 00 00 00 00 10 00 03 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	0c 00 00 00 10 00 00 00 ]
[sof]"ipc tx: 0x50040000: GLB_COMP_MSG: GET_DATA\n"
[sof]"ipc tx succeeded: 0x50040000: GLB_COMP_MSG: GET_DATA\n"
[sof]"widget CODEC_ADAPTER1.0 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	2c 00 00 00 00 00 20 30 02 00 00 00 0c 00 00 00 
	01 00 00 00 00 00 00 00 00 00 00 00 00 10 00 00 
	61 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"ipc tx succeeded: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"widget BUF1.0 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	2c 00 00 00 00 00 20 30 03 00 00 00 0c 00 00 00 
	01 00 00 00 00 00 00 00 00 00 00 00 00 10 00 00 
	61 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"ipc tx succeeded: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"widget BUF1.1 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	60 00 00 00 00 00 01 30 04 00 00 00 02 00 00 00 
	01 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 03 00 00 00 05 00 00 00 00 00 00 00 
	27 0d b0 c2 bc ff 50 41 a5 1a 24 5c 79 c5 e5 4b ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[00:00:00.834,000] <inf> usb_dc_csk: EP1 RX fifo adress is 0x8, size is 0x40
uart:~$ [sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"[sof_dai_config_setup 85]config setup dai_index 3 type 5\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	d8 00 00 00 00 00 01 80 05 00 00 00 03 00 00 00 
	01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 
	00 80 bb 00 80 bb 00 00 00 e0 2e 00 02 00 00 00 
	03 00 00 00 03 00 00 00 20 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x80010000: GLB_DAI_MSG: CONFIG\n"
[sof]"ipc tx succeeded: 0x80010000: GLB_DAI_MSG: CONFIG\n"
[sof]"widget SAI3.OUT setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	30 00 00 00 00 00 10 30 05 00 00 00 01 00 00 00 
	04 00 00 00 00 00 00 00 e8 03 00 00 00 00 00 00 
	88 13 00 00 00 00 00 00 00 00 00 00 01 00 00 00 ]
[sof]"ipc tx: 0x30100000: GLB_TPLG_MSG: PIPE_NEW\n"
[sof]"ipc tx succeeded: 0x30100000: GLB_TPLG_MSG: PIPE_NEW\n"
[sof]"widget PIPELINE.1.SAI3.OUT setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	5c 00 00 00 00 00 01 30 06 00 00 00 01 00 00 00 
	02 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	01 00 00 00 00 00 00 00 00 00 00 00 0c 10 9d 8b 
	78 6d 8f 41 90 a3 e0 e8 05 d0 85 2b ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"widget PCM0C setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	64 00 00 00 00 00 01 30 07 00 00 00 05 00 00 00 
	02 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 02 00 00 00 04 00 00 00 00 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	02 00 00 00 00 00 00 00 c7 a0 1f 00 00 00 00 00 
	fa 00 00 00 7e 67 7e b7 f4 5f 88 41 af 14 fb a8 
	bd bf 86 82 ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	6c 00 00 00 00 00 01 50 00 00 00 00 07 00 00 00 
	01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 01 00 01 00 00 00 00 00 01 00 ]
[sof]"ipc tx: 0x50010000: GLB_COMP_MSG: SET_VALUE\n"
[sof]"ipc tx succeeded: 0x50010000: GLB_COMP_MSG: SET_VALUE\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	6c 00 00 00 00 00 02 50 00 00 00 00 07 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 02 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 01 00 01 00 00 00 00 00 01 00 ]
[sof]"ipc tx: 0x50020000: GLB_COMP_MSG: GET_VALUE\n"
[sof]"ipc tx succeeded: 0x50020000: GLB_COMP_MSG: GET_VALUE\n"
[sof]"widget PGA2.0 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	2c 00 00 00 00 00 20 30 08 00 00 00 0c 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 10 00 00 
	61 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"ipc tx succeeded: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"widget BUF2.0 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	2c 00 00 00 00 00 20 30 09 00 00 00 0c 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 10 00 00 
	61 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"ipc tx succeeded: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"widget BUF2.1 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	60 00 00 00 00 00 01 30 0a 00 00 00 02 00 00 00 
	02 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	01 00 00 00 03 00 00 00 05 00 00 00 00 00 00 00 
	27 0d b0 c2 bc ff 50 41 a5 1a 24 5c 79 c5 e5 4b ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"[sof_dai_config_setup 85]config setup dai_index 3 type 5\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	d8 00 00 00 00 00 01 80 05 00 00 00 03 00 00 00 
	01 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 01 00 00 00 
	00 80 bb 00 80 bb 00 00 00 e0 2e 00 02 00 00 00 
	03 00 00 00 03 00 00 00 20 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x80010000: GLB_DAI_MSG: CONFIG\n"
[sof]"ipc tx succeeded: 0x80010000: GLB_DAI_MSG: CONFIG\n"
[sof]"widget SAI3.IN setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	30 00 00 00 00 00 10 30 0b 00 00 00 02 00 00 00 
	0a 00 00 00 00 00 00 00 e8 03 00 00 00 00 00 00 
	88 13 00 00 00 00 00 00 00 00 00 00 01 00 00 00 ]
[sof]"ipc tx: 0x30100000: GLB_TPLG_MSG: PIPE_NEW\n"
[sof]"ipc tx succeeded: 0x30100000: GLB_TPLG_MSG: PIPE_NEW\n"
[sof]"widget PIPELINE.2.SAI3.IN setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	5c 00 00 00 00 00 01 30 0c 00 00 00 01 00 00 00 
	03 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 00 00 00 00 03 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	01 00 00 00 00 00 00 00 00 00 00 00 0c 10 9d 8b 
	78 6d 8f 41 90 a3 e0 e8 05 d0 85 2b ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"widget PCM3C setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	5c 00 00 00 00 00 01 30 0d 00 00 00 08 00 00 00 
	03 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 04 00 00 00 03 00 00 00 00 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	80 bb 00 00 00 00 00 00 00 00 00 00 6d 32 c5 c1 
	90 83 b4 46 aa 47 95 c3 be ca 65 50 ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"widget SRC3.0 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	2c 00 00 00 00 00 20 30 0e 00 00 00 0c 00 00 00 
	03 00 00 00 00 00 00 00 00 00 00 00 00 10 00 00 
	61 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"ipc tx succeeded: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"widget BUF3.0 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	2c 00 00 00 00 00 20 30 0f 00 00 00 0c 00 00 00 
	03 00 00 00 00 00 00 00 00 00 00 00 00 10 00 00 
	61 00 00 00 00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"ipc tx succeeded: 0x30200000: GLB_TPLG_MSG: BUFFER_NEW\n"
[sof]"widget BUF3.1 setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	60 00 00 00 00 00 01 30 10 00 00 00 02 00 00 00 
	03 00 00 00 00 00 00 00 10 00 00 00 24 00 00 00 
	00 00 00 00 02 00 00 00 00 00 00 00 00 00 00 00 
	02 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	01 00 00 00 00 00 00 00 01 00 00 00 00 00 00 00 
	27 0d b0 c2 bc ff 50 41 a5 1a 24 5c 79 c5 e5 4b ]
[sof]"ipc tx: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"ipc tx succeeded: 0x30010000: GLB_TPLG_MSG: COMP_NEW\n"
[sof]"[sof_dai_config_setup 85]config setup dai_index 0 type 1\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	d8 00 00 00 00 00 01 80 01 00 00 00 00 00 00 00 
	01 40 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 f8 24 01 
	80 bb 00 00 00 4c 1d 00 02 00 00 00 03 00 00 00 
	03 00 00 00 10 00 00 00 14 00 00 00 01 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 
	00 00 00 00 00 00 00 00 ]
[sof]"ipc tx: 0x80010000: GLB_DAI_MSG: CONFIG\n"
[sof]"ipc tx succeeded: 0x80010000: GLB_DAI_MSG: CONFIG\n"
[sof]"widget SSP0.IN setup complete\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	30 00 00 00 00 00 10 30 11 00 00 00 03 00 00 00 
	10 00 00 00 00 00 00 00 e8 03 00 00 00 00 00 00 
	88 13 00 00 00 00 00 00 00 00 00 00 01 00 00 00 ]
[sof]"ipc tx: 0x30100000: GLB_TPLG_MSG: PIPE_NEW\n"
[sof]"ipc tx succeeded: 0x30100000: GLB_TPLG_MSG: PIPE_NEW\n"
[sof]"widget PIPELINE.3.SSP0.IN setup complete\n"
[sof]"setting up route PCM0P -> BUF1.0\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 00 00 00 00 02 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route BUF1.0 -> CODEC_ADAPTER1.0\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 02 00 00 00 01 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route CODEC_ADAPTER1.0 -> BUF1.1\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 01 00 00 00 03 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route BUF1.1 -> SAI3.OUT\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 03 00 00 00 04 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route BUF2.0 -> PCM0C\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 08 00 00 00 06 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route PGA2.0 -> BUF2.0\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 07 00 00 00 08 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route BUF2.1 -> PGA2.0\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 09 00 00 00 07 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route SAI3.IN -> BUF2.1\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 0a 00 00 00 09 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route BUF3.0 -> PCM3C\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 0e 00 00 00 0c 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route SRC3.0 -> BUF3.0\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 0d 00 00 00 0e 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route BUF3.1 -> SRC3.0\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 0f 00 00 00 0d 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"setting up route SSP0.IN -> BUF3.1\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	10 00 00 00 00 00 03 30 10 00 00 00 0f 00 00 00 ]
[sof]"ipc tx: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"ipc tx succeeded: 0x30030000: GLB_TPLG_MSG: COMP_CONNECT\n"
[sof]"tplg: complete pipeline PIPELINE.3.SSP0.IN id 17\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	0c 00 00 00 00 00 13 30 11 00 00 00 ]
[sof]"ipc tx: 0x30130000: GLB_TPLG_MSG: PIPE_COMPLETE\n"
[sof]"ipc tx succeeded: 0x30130000: GLB_TPLG_MSG: PIPE_COMPLETE\n"
[sof]"tplg: complete pipeline PIPELINE.2.SAI3.IN id 11\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	0c 00 00 00 00 00 13 30 0b 00 00 00 ]
[sof]"ipc tx: 0x30130000: GLB_TPLG_MSG: PIPE_COMPLETE\n"
[sof]"ipc tx succeeded: 0x30130000: GLB_TPLG_MSG: PIPE_COMPLETE\n"
[sof]"tplg: complete pipeline PIPELINE.1.SAI3.OUT id 5\n"
[sof]"[csk6_send_msg 220]container IPC MASSAGE data type 0:["

	0c 00 00 00 00 00 13 30 05 00 00 00 ]
[sof]"ipc tx: 0x30130000: GLB_TPLG_MSG: PIPE_COMPLETE\n"
[sof]"ipc tx succeeded: 0x30130000: GLB_TPLG_MSG: PIPE_COMPLETE\n"
