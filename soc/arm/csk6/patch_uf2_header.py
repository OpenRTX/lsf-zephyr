#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 LISTENAI
#
# SPDX-License-Identifier: Apache-2.0

import sys
import struct

IMAGE_START = 192
IMAGE_SIZE_OFFSET = IMAGE_START + 4
IMAGE_HDR_CHK_OFFSET = IMAGE_START + 60

UF2_PAYLOAD_START = 32

def read_bin_header(filename):
    with open(filename, 'rb') as bin_file:
        bin_file.seek(IMAGE_SIZE_OFFSET, 0)
        [image_size] = struct.unpack('<I', bin_file.read(4))

        bin_file.seek(IMAGE_HDR_CHK_OFFSET, 0)
        [header_sum, vector_sum] = struct.unpack('<HH', bin_file.read(4))

        return image_size, header_sum, vector_sum

def patch_uf2(filename, image_size, header_sum, vector_sum):
    with open(filename, 'rb+') as uf2_file:
        uf2_file.seek(UF2_PAYLOAD_START + IMAGE_SIZE_OFFSET , 0)
        uf2_file.write(image_size.to_bytes(4, 'little'))
        uf2_file.seek(UF2_PAYLOAD_START + IMAGE_HDR_CHK_OFFSET, 0)
        uf2_file.write(header_sum.to_bytes(2, 'little'))
        uf2_file.write(vector_sum.to_bytes(2, 'little'))

def main():
    image_size, header_sum, vector_sum = read_bin_header(sys.argv[1])
    patch_uf2(sys.argv[2], image_size, header_sum, vector_sum)

if __name__ == "__main__":
    main()
