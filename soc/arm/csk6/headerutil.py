#!/usr/bin/env python3
#
# Copyright (c) 2021-2022 LISTENAI
#
# SPDX-License-Identifier: Apache-2.0

_IMAGE_START = 192
_IMAGE_SIZE_OFFSET = _IMAGE_START + 4
_IMAGE_HDR_CHK_OFFSET = _IMAGE_START + 60

HEADER_SIZE = _IMAGE_HDR_CHK_OFFSET + 4

def _get_header_sum(header_bytes: bytearray):
    header_sum = 0
    for pos in range(_IMAGE_START, _IMAGE_HDR_CHK_OFFSET):
        header_sum += header_bytes[pos]

    vector_sum = 0
    for pos in range(_IMAGE_HDR_CHK_OFFSET):
        vector_sum += header_bytes[pos]
    vector_sum += (header_sum) & 0xff
    vector_sum += (header_sum >> 8) & 0xff

    return header_sum, vector_sum

def patch_header(header_bytes: bytearray, image_size: int):
    header_bytes[_IMAGE_SIZE_OFFSET:_IMAGE_SIZE_OFFSET + 4] = image_size.to_bytes(4, 'little')

    header_sum, vector_sum = _get_header_sum(header_bytes)
    header_bytes[_IMAGE_HDR_CHK_OFFSET + 0:_IMAGE_HDR_CHK_OFFSET + 2] = header_sum.to_bytes(2, 'little')
    header_bytes[_IMAGE_HDR_CHK_OFFSET + 2:_IMAGE_HDR_CHK_OFFSET + 4] = vector_sum.to_bytes(2, 'little')
