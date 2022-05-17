##########################################################################
# File Name: burn.sh
# Author: ThierryCao
# mail: iamthinker@163.com
# Created Time: Tue 10 May 2022 01:38:48 PM HKT
#########################################################################
#!/bin/bash
cskburn -C 6 -s /dev/ttyUSB0 0x18000000 build/zephyr/zephyr.bin
