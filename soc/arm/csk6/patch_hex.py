#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 LISTENAI
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys
from intelhex import IntelHex
from headerutil import HEADER_SIZE, patch_header

CSK_FLASH_START = 0x18000000

def patch_hex(filename: str):
    hex_file = IntelHex(filename)
    header = bytearray(hex_file.gets(CSK_FLASH_START, HEADER_SIZE))
    image_size = hex_file.maxaddr() - CSK_FLASH_START + 1
    patch_header(header, image_size)
    hex_file.puts(CSK_FLASH_START, bytes(header))
    hex_file.write_hex_file(filename)

def main():
    patch_hex(sys.argv[1])

if __name__ == "__main__":
    main()
