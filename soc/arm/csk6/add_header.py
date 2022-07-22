#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 LISTENAI
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys

IMAGE_START = 192
IMAGE_SIZE_OFFSET = IMAGE_START + 4
IMAGE_HDR_CHK_OFFSET = IMAGE_START + 60

def get_header_sum(header):
    header_sum = 0
    for pos in range(IMAGE_START, IMAGE_HDR_CHK_OFFSET):
        header_sum += header[pos]

    vector_sum = 0
    for pos in range(IMAGE_HDR_CHK_OFFSET):
        vector_sum += header[pos]
    vector_sum += (header_sum) & 0xff
    vector_sum += (header_sum >> 8) & 0xff

    return header_sum, vector_sum

def verify_and_writeback_bin(filename):
    with open(filename, 'rb+') as bin_file:
        image_size = os.path.getsize(filename)
        bin_file.seek(IMAGE_SIZE_OFFSET, 0)
        bin_file.write(image_size.to_bytes(4, 'little'))

        bin_file.seek(0, 0)
        header = bin_file.read(IMAGE_HDR_CHK_OFFSET)
        header_sum, vector_sum = get_header_sum(header)
        bin_file.write(header_sum.to_bytes(2, 'little'))
        bin_file.write(vector_sum.to_bytes(2, 'little'))

def main():
    verify_and_writeback_bin(sys.argv[1])

if __name__ == "__main__":
    main()
