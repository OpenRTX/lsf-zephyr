#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 LISTENAI
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys

from headerutil import HEADER_SIZE, patch_header

UF2_PAYLOAD_START = 32

def patch_uf2(filename: str, image_size: int):
    with open(filename, 'rb+') as uf2_file:
        uf2_file.seek(UF2_PAYLOAD_START, 0)
        header = bytearray(uf2_file.read(HEADER_SIZE))
        patch_header(header, image_size)
        uf2_file.seek(UF2_PAYLOAD_START, 0)
        uf2_file.write(header)

def main():
    image_size = os.path.getsize(sys.argv[2])
    patch_uf2(sys.argv[1], image_size)

if __name__ == "__main__":
    main()
