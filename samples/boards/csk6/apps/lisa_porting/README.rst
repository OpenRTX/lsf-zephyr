.. _littlefs-sample:

lisa_porting  Sample Application
#######################################


A  simple lisa_porting sample, with http and websocket. 


Overview
********


.. zephyr-app-commands::
   :zephyr-app: samples/boards/csk6/apps/lisa_porting
   :board: csk6
   :goals: build
   :compact:


.. _cdc-acm-console:


Requirements
************

This project requires a USB device controller driver.

Building and Running
********************


Make sure you have the csk6002_c3_nano connected over Jlink debuger.

The sample can be built and flashed as follows:

.. code-block:: console
    
    lisa zep  build -b csk6002_c3_nano -t menuconfig 
    lisa zep  build -b csk6002_c3_nano 
    lisa zep flash --runner pyocd

Plug the board into a host device, for sample, a PC running Linux OS.
The board will be detected as a CDC_ACM serial device. To see the console output
from the sample, use a command similar to "minicom -D /dev/ttyACM0".

.. code-block:: console

   *** Booting Zephyr OS build 8baecfe52492  ***
    lisa porting test on csk6002_c3_nano !
    connecting to wifi: TP-LINK_BA83 ...
    [WiFi sta] connected
    Your address: 192.168.71.213
    Lease time: 3599 seconds
    Subnet: 255.255.255.0
    Router: 192.168.71.235
    [D][HTTP_CLIENT]set post file length = 115
    [D][HTTP_CLIENT]Begin connect to: http://www.rt-thread.com:80
    [D][TRANS_TCP][sock=0],connecting to server IP: 118.31.15.152, port: 80 
    [D][HTTP_CLIENT]ze_http_client_request_send 1145
    [D][HTTP_CLIENT]ze_http_client_request_send 1164
    [D][HTTP_CLIENT]ze_http_client_request_send 1178
    [D][HTTP_CLIENT]ze_http_client_request_send 1196
    [D][HTTP_CLIENT]ze_http_client_send_post_data start
    [D][HTTP_CLIENT]ze_http_client_send_post_data end
    [D][HTTP_CLIENT]ze_http_client_fetch_headers start
    [D][HTTP_CLIENT]on_message_begin
    [D][HTTP_CLIENT]HEADER=Server:nginx/1.10.3 (Ubuntu)
    [D][HTTP_CLIENT]HEADER=Date:Thu, 14 Apr 2022 16:43:26 GMT
    [D][HTTP_CLIENT]HEADER=Content-Type:text/html
    [D][HTTP_CLIENT]HEADER=Content-Length:115
    [D][HTTP_CLIENT]HEADER=Connection:keep-alive
    [D][HTTP_CLIENT]HEADER=Vary:Accept-Encoding
    [D][HTTP_CLIENT]HEADER=X-Powered-By:PHP/5.5.9-1ubuntu4.25
    [D][HTTP_CLIENT]HEADER=Vary:Accept-Encoding
    [D][HTTP_CLIENT]http_on_headers_complete, status=200, offset=240, nread=240
    [D][hal_http]HTTP_EVENT_ON_DATA, len=115
    [E][http_test]http response : len 115
    [D][HTTP_CLIENT]http_on_message_complete, parser=a3670
    [D][HTTP_CLIENT]ze_http_client_fetch_headers end

