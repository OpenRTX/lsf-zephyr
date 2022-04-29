.. _wifi_mgr:

CSK6002 WiFi Manager Example
############################

Overview
********

This sample demonstrates how to use csk6002 WiFi Manager to scan the neighboring AP device and auto connect to a WiFi
AP as a station device, and save the connected AP information into WiFi NVS storage.

Building and Running
********************

Make sure you have the CSK6002 connected over USB port.

The sample can be built and flashed as follows:

.. code-block:: console

   west build -b csk6002_c3_nano samples/boards/csk6/wifi_mgr
   west flash

Sample Output
=============

To check output of this sample, any serial console program can be used (i.e. on Linux minicom, putty, screen, etc)
This example uses ``minicom`` on the serial port ``/dev/ttyUSB0``:

.. code-block:: console

   $ minicom -D /dev/ttyUSB0 -b 115200

.. code-block:: console

   *** Booting Zephyr OS build 886f80e3d17f  ***
   [00:00:01.531,000] <dbg> fs_nvs.nvs_recover_last_ate: Recovering last ate from sector 2
   [00:00:01.534,000] <inf> fs_nvs: 5 Sectors of 4096 bytes
   [00:00:01.534,000] <inf> fs_nvs: alloc wra: 2, fa0
   [00:00:01.534,000] <inf> fs_nvs: data wra: 2, 10c
   [00:00:01.535,000] <inf> csk6_spi: CS control inhibited (no GPIO device)
   WiFi scan done
   -------------------------------Scan AP list---------------------------------
   |ssid: Xiaomi-LINGSI_2.4G   | bssid: 3c:cd:57:f4:d5:6a  | channel: 6   | rssi: -15|
   |ssid: TreeNewbee           | bssid: d4:35:38:a3:e0:6f  | channel: 13  | rssi: -32|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:2c:51  | channel: 6   | rssi: -42|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:0c:31  | channel: 6   | rssi: -42|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:2c:50  | channel: 6   | rssi: -43|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:0c:30  | channel: 6   | rssi: -43|
   |ssid: TP-LINK_LINGSI       | bssid: 34:96:72:e7:8e:20  | channel: 1   | rssi: -48|
   |ssid:                      | bssid: b8:c3:85:74:14:b1  | channel: 1   | rssi: -51|
   |ssid: LINGSI               | bssid: 94:3b:b0:62:71:b0  | channel: 6   | rssi: -54|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:62:71:b1  | channel: 6   | rssi: -55|
   |ssid: xintiao-2.4          | bssid: 80:ea:07:69:8a:37  | channel: 11  | rssi: -56|
   |ssid: ChinaNet-DEtt        | bssid: e8:84:c6:1e:48:88  | channel: 11  | rssi: -61|
   |ssid: cxdx                 | bssid: 08:10:7c:43:da:6b  | channel: 11  | rssi: -63|
   |ssid: RIVAASTADIUM0FB9C9   | bssid: ce:90:93:0f:b9:c9  | channel: 1   | rssi: -64|
   |ssid: MARS                 | bssid: 08:31:a4:39:a4:78  | channel: 11  | rssi: -67|
   |ssid: ChinaNet-UH3m        | bssid: 7c:76:30:1c:f0:40  | channel: 9   | rssi: -71|
   |ssid: chenyangzhi          | bssid: 32:7c:7d:72:55:ab  | channel: 1   | rssi: -75|
   ----------------------------------------------------------------------------
   ---------------------------wifi storage info list---------------------------
   ----------------------------------------------------------------------------
   WiFi connecting to AP ...
   Connected to AP successfully
   ssid: TreeNewbee  bssid: d4:35:38:a3:e0:6f  channel: 13  rssi: -33
   WiFi saving AP info into NVS storage
   Save item, ssid: TreeNewbee, addr: 0x81e28, pwd: wifishare123@, bssid: d4:35:38:a3:e0:6f
   Your address: 192.168.28.51
   Lease time: 43200 seconds
   Subnet: 255.255.255.0
   Router: 192.168.28.1
   Waiting for Reboot ...3
   Waiting for Reboot ...2
   Waiting for Reboot ...1
   Waiting for Reboot ...0
   *** Booting Zephyr OS build 886f80e3d17f  ***
   [00:00:01.541,000] <dbg> fs_nvs.nvs_recover_last_ate: Recovering last ate from sector 2
   [00:00:01.544,000] <inf> fs_nvs: 5 Sectors of 4096 bytes
   [00:00:01.544,000] <inf> fs_nvs: alloc wra: 2, f88
   [00:00:01.544,000] <inf> fs_nvs: data wra: 2, 194
   [00:00:01.544,000] <inf> csk6_spi: CS control inhibited (no GPIO device)
   WiFi scan done
   -------------------------------Scan AP list---------------------------------
   |ssid: Xiaomi-LINGSI_2.4G   | bssid: 3c:cd:57:f4:d5:6a  | channel: 6   | rssi: -14|
   |ssid: TreeNewbee           | bssid: d4:35:38:a3:e0:6f  | channel: 13  | rssi: -31|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:2c:50  | channel: 6   | rssi: -41|
   |ssid: TP-LINK_LINGSI       | bssid: 34:96:72:e7:8e:20  | channel: 1   | rssi: -42|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:0c:31  | channel: 6   | rssi: -42|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:0c:30  | channel: 6   | rssi: -43|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:2c:51  | channel: 6   | rssi: -43|
   |ssid:                      | bssid: b8:c3:85:74:14:b1  | channel: 1   | rssi: -46|
   |ssid: HUAWEI-验收环境  | bssid: b8:c3:85:74:14:b0  | channel: 1   | rssi: -46|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:62:71:b1  | channel: 6   | rssi: -52|
   |ssid: LINGSI               | bssid: 94:3b:b0:62:71:b0  | channel: 6   | rssi: -54|
   |ssid: ChinaNet-DEtt        | bssid: e8:84:c6:1e:48:88  | channel: 11  | rssi: -58|
   |ssid: xintiao-2.4          | bssid: 80:ea:07:69:8a:37  | channel: 11  | rssi: -59|
   |ssid: cxdx                 | bssid: 08:10:7c:43:da:6b  | channel: 11  | rssi: -60|
   |ssid: RIVAASTADIUM0FB9C9   | bssid: ce:90:93:0f:b9:c9  | channel: 1   | rssi: -64|
   |ssid: ChinaNet-nUec        | bssid: a0:df:15:8c:a6:d0  | channel: 1   | rssi: -65|
   |ssid: MARS                 | bssid: 08:31:a4:39:a4:78  | channel: 11  | rssi: -67|
   |ssid: ChinaNet-uUsy        | bssid: 48:a7:3c:f7:c2:33  | channel: 9   | rssi: -74|
   |ssid: chenyangzhi          | bssid: 32:7c:7d:72:55:ab  | channel: 1   | rssi: -77|
   |ssid: ChinaNet-FnWS        | bssid: e8:84:c6:1d:bd:9c  | channel: 11  | rssi: -81|
   |ssid: ChinaNet-8ySB        | bssid: 6e:ef:c6:37:c5:96  | channel: 6   | rssi: -83|
   |ssid:                      | bssid: 3e:ef:c6:37:c5:96  | channel: 6   | rssi: -83|
   |ssid: ae                   | bssid: 00:e0:4c:81:ae:98  | channel: 11  | rssi: -84|
   ----------------------------------------------------------------------------
   ---------------------------wifi storage info list---------------------------
   |ssid: TreeNewbee           | bssid: d4:35:38:a3:e0:6f  | channel: 13  | rssi: -33|
   ----------------------------------------------------------------------------
   Found ssid in NVS storage, ssid: TreeNewbee, bssid: d4:35:38:a3:e0:6f, password: wifishare123@
   WiFi auto connect start ...
   WiFi scan done
   Connected to AP successfully
   ssid: TreeNewbee  bssid: d4:35:38:a3:e0:6f  channel: 13  rssi: -32
   Your address: 192.168.28.51
   Lease time: 43200 seconds
   Subnet: 255.255.255.0
   Router: 192.168.28.1
   Waiting for Reboot ...3
   Waiting for Reboot ...2
   Waiting for Reboot ...1
   Waiting for Reboot ...0
   *** Booting Zephyr OS build 886f80e3d17f  ***
   [00:00:01.521,000] <dbg> fs_nvs.nvs_recover_last_ate: Recovering last ate from sector 2
   [00:00:01.524,000] <inf> fs_nvs: 5 Sectors of 4096 bytes
   [00:00:01.524,000] <inf> fs_nvs: alloc wra: 2, f88
   [00:00:01.524,000] <inf> fs_nvs: data wra: 2, 194
   [00:00:01.525,000] <inf> csk6_spi: CS control inhibited (no GPIO device)
   WiFi scan done
   -------------------------------Scan AP list---------------------------------
   |ssid: Xiaomi-LINGSI_2.4G   | bssid: 3c:cd:57:f4:d5:6a  | channel: 6   | rssi: -14|
   |ssid: TreeNewbee           | bssid: d4:35:38:a3:e0:6f  | channel: 13  | rssi: -31|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:2c:51  | channel: 6   | rssi: -41|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:0c:30  | channel: 6   | rssi: -42|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:2c:50  | channel: 6   | rssi: -42|
   |ssid: TP-LINK_LINGSI       | bssid: 34:96:72:e7:8e:20  | channel: 1   | rssi: -43|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:0c:31  | channel: 6   | rssi: -44|
   |ssid:                      | bssid: b8:c3:85:74:14:b1  | channel: 1   | rssi: -52|
   |ssid: HUAWEI-验收环境  | bssid: b8:c3:85:74:14:b0  | channel: 1   | rssi: -53|
   |ssid: LINGSI               | bssid: 94:3b:b0:62:71:b0  | channel: 6   | rssi: -55|
   |ssid: xintiao-2.4          | bssid: 80:ea:07:69:8a:37  | channel: 11  | rssi: -56|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:62:71:b1  | channel: 6   | rssi: -57|
   |ssid: ChinaNet-DEtt        | bssid: e8:84:c6:1e:48:88  | channel: 11  | rssi: -58|
   |ssid: ChinaNet-RZSy        | bssid: e8:84:c6:1e:44:b0  | channel: 6   | rssi: -65|
   |ssid: MARS                 | bssid: 08:31:a4:39:a4:78  | channel: 11  | rssi: -65|
   |ssid: HP-Print-96-LaserJet Pro MFP | bssid: 90:cd:b6:3b:3e:96  | channel: 11  | rssi: -68|
   |ssid: ChinaNet-nUec        | bssid: a0:df:15:8c:a6:d0  | channel: 1   | rssi: -70|
   |ssid: ES-083963            | bssid: ac:64:cf:ba:f4:68  | channel: 1   | rssi: -73|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:05:50  | channel: 11  | rssi: -74|
   |ssid: ChinaNet-uUsy        | bssid: 48:a7:3c:f7:c2:33  | channel: 9   | rssi: -75|
   |ssid: ChinaNet-UH3m        | bssid: 7c:76:30:1c:f0:40  | channel: 9   | rssi: -75|
   |ssid: YDW_03               | bssid: 48:7d:2e:7c:74:61  | channel: 1   | rssi: -81|
   |ssid: 是谁送你来到我身边 | bssid: 00:36:76:32:49:dc  | channel: 6   | rssi: -89|
   ----------------------------------------------------------------------------
   ---------------------------wifi storage info list---------------------------
   |ssid: TreeNewbee           | bssid: d4:35:38:a3:e0:6f  | channel: 13  | rssi: -33|
   ----------------------------------------------------------------------------
   Found ssid in NVS storage, ssid: TreeNewbee, bssid: d4:35:38:a3:e0:6f, password: wifishare123@
   WiFi auto connect start ...
   WiFi scan done
   Connected to AP successfully
   ssid: TreeNewbee  bssid: d4:35:38:a3:e0:6f  channel: 13  rssi: -31
   Your address: 192.168.28.51
   Lease time: 43200 seconds
   Subnet: 255.255.255.0
   Router: 192.168.28.1
   Waiting for Reboot ...3
   Waiting for Reboot ...2
   Waiting for Reboot ...1
   Waiting for Reboot ...0
   *** Booting Zephyr OS build 886f80e3d17f  ***
   [00:00:01.531,000] <dbg> fs_nvs.nvs_recover_last_ate: Recovering last ate from sector 2
   [00:00:01.534,000] <inf> fs_nvs: 5 Sectors of 4096 bytes
   [00:00:01.534,000] <inf> fs_nvs: alloc wra: 2, f88
   [00:00:01.534,000] <inf> fs_nvs: data wra: 2, 194
   [00:00:01.534,000] <inf> csk6_spi: CS control inhibited (no GPIO device)
   WiFi scan done
   -------------------------------Scan AP list---------------------------------
   |ssid: Xiaomi-LINGSI_2.4G   | bssid: 3c:cd:57:f4:d5:6a  | channel: 6   | rssi: -14|
   |ssid: TreeNewbee           | bssid: d4:35:38:a3:e0:6f  | channel: 13  | rssi: -32|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:2c:51  | channel: 6   | rssi: -39|
   |ssid: TP-LINK_LINGSI       | bssid: 34:96:72:e7:8e:20  | channel: 1   | rssi: -40|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:2c:50  | channel: 6   | rssi: -40|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:0c:30  | channel: 6   | rssi: -43|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:0c:31  | channel: 6   | rssi: -43|
   |ssid:                      | bssid: b8:c3:85:74:14:b1  | channel: 1   | rssi: -50|
   |ssid: HUAWEI-验收环境  | bssid: b8:c3:85:74:14:b0  | channel: 1   | rssi: -51|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:62:71:b1  | channel: 6   | rssi: -54|
   |ssid: xintiao-2.4          | bssid: 80:ea:07:69:8a:37  | channel: 11  | rssi: -54|
   |ssid: LINGSI               | bssid: 94:3b:b0:62:71:b0  | channel: 6   | rssi: -56|
   |ssid: ChinaNet-DEtt        | bssid: e8:84:c6:1e:48:88  | channel: 11  | rssi: -60|
   |ssid: RIVAASTADIUM0FB9C9   | bssid: ce:90:93:0f:b9:c9  | channel: 1   | rssi: -61|
   |ssid: cxdx                 | bssid: 08:10:7c:43:da:6b  | channel: 11  | rssi: -62|
   |ssid: MARS                 | bssid: 08:31:a4:39:a4:78  | channel: 11  | rssi: -64|
   |ssid: ChinaNet-RZSy        | bssid: e8:84:c6:1e:44:b0  | channel: 6   | rssi: -66|
   |ssid: ChinaNet-nUec        | bssid: a0:df:15:8c:a6:d0  | channel: 1   | rssi: -71|
   |ssid: ChinaNet-uUsy        | bssid: 48:a7:3c:f7:c2:33  | channel: 9   | rssi: -73|
   |ssid: ChinaNet-UH3m        | bssid: 7c:76:30:1c:f0:40  | channel: 9   | rssi: -74|
   |ssid: Mars-cwb             | bssid: d4:ee:07:43:d1:94  | channel: 8   | rssi: -75|
   |ssid: LINGSI               | bssid: 94:3b:b0:63:05:50  | channel: 11  | rssi: -75|
   |ssid: LINGSI_2.4G          | bssid: 94:3b:b0:63:05:51  | channel: 11  | rssi: -75|
   |ssid: Redmi_5A22           | bssid: 5c:02:14:f3:e0:cc  | channel: 11  | rssi: -81|
   ----------------------------------------------------------------------------
   ---------------------------wifi storage info list---------------------------
   |ssid: TreeNewbee           | bssid: d4:35:38:a3:e0:6f  | channel: 13  | rssi: -33|
   ----------------------------------------------------------------------------
   Found ssid in NVS storage, ssid: TreeNewbee, bssid: d4:35:38:a3:e0:6f, password: wifishare123@
   WiFi auto connect start ...
   WiFi scan done
   Connected to AP successfully
   ssid: TreeNewbee  bssid: d4:35:38:a3:e0:6f  channel: 13  rssi: -31
   Your address: 192.168.28.51
   Lease time: 43200 seconds
   Subnet: 255.255.255.0
   Router: 192.168.28.1
   Waiting for Reboot ...3
   Waiting for Reboot ...2
   Waiting for Reboot ...1
   Waiting for Reboot ...0

