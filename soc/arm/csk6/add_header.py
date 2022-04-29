#!/usr/bin/env python3
#
# Copyright (c) 2021 LISTENAI
#
# SPDX-License-Identifier: Apache-2.0

import os
import sys

def get_vec_hdr_sum(target_bytes, start_pos):
    vector_sum = 0
    header_sum = 0
    for byte_pos in range(len(target_bytes)):
        if byte_pos < start_pos:
            vector_sum += target_bytes[byte_pos]
        else:
            header_sum += target_bytes[byte_pos]

    vector_sum += header_sum + (header_sum & 0xff) + (header_sum >> 8)
    return vector_sum, header_sum

def verify_and_writeback_bin(filename):
    image_start_pos = 192
    img_str_to_hdr_chesum_offset = 60
    img_str_to_vec_chesum_offset = 62
    image_size_pos = image_start_pos + 4
    image_header_checksum_pos = image_start_pos + img_str_to_hdr_chesum_offset

    bin_file = open(filename, 'rb+')
    bin_file_size = os.path.getsize(filename)

    bin_file.seek(image_size_pos, 0)
    bin_file.write(bin_file_size.to_bytes(4, 'little'))  # write image size

    bin_file.seek(0, 0)
    target_bytes = bin_file.read(image_header_checksum_pos)
    vector_sum, header_sum = get_vec_hdr_sum(
        target_bytes, image_start_pos)
    # write header checksum
    bin_file.write(header_sum.to_bytes(2, 'little'))
    bin_file.write(vector_sum.to_bytes(2, 'little'))
    bin_file.close()

def main():
    verify_and_writeback_bin(sys.argv[1])

if __name__ == "__main__":
    main()
