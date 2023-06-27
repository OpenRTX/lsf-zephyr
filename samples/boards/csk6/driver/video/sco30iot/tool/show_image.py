#!/usr/bin/evn python3
# -*- coding: UTF-8 -*-

import serial
import threading
import serial.tools.list_ports
import datetime
import time
import numpy as np
import cv2
import sys
import getopt

# import Image
from PIL import Image
import matplotlib.pyplot as pyplot

import logging


class ShowImage():

    log_file_name = 'log_debug'

    CAMERA_MODE_YUV422 = 0          # YUV422
    CAMERA_MODE_YUV420 = 1          # YUV420
    CAMERA_MODE_Y_ONLY = 2          # Y only
    CAMERA_MODE_RAW_DATA = 3        # Raw data
    CAMERA_MODE_RGB565 = 4          # RGB565
    CAMERA_MODE_YUV422_FIX_UV = 5   # YUV422 with fixed UV

    img_data = []
    img_wd = 0
    img_ht = 0
    img_md = 0
    img_vd = False

    img_refresh = 10

    # The constructor.
    def __init__(self, parent=None):
        self.create_logger()
        # cv2.startWindowThread()
        cv2.destroyAllWindows()

    def create_logger(self):
        log_lvl = logging.WARNING  # logging.ERROR#logging.DEBUG #logging.INFO #

        formatter = '%(asctime)s [%(levelname)s] %(filename)s - %(funcName)s(Line %(lineno)d)： %(message)s'
        logging.basicConfig(level=log_lvl, format=formatter)
        self.logger = logging.getLogger('image_show')

        # if not defined the 'log_file_name', use the varied time as the file name
        if self.log_file_name == '':
            from datetime import datetime
            log_file = "log"+(datetime.now().strftime('_%Y%m%d%H%M%S'))
        else:
            log_file = self.log_file_name

        fh = logging.FileHandler(log_file, 'w')

        fh.setFormatter(logging.Formatter(formatter))
        self.logger.addHandler(fh)

    # V Y U Y
    def YUVtoRGB(self, byteArray, width, height):
        Y = byteArray[1::2]
        Y = np.reshape(Y, (height, width))

        U = byteArray[0::4]
        U = np.repeat(U, 2, 0)
        U = np.reshape(U, (height, width))
        # U = np.repeat(U, 2, 0)

        V = byteArray[2::4]
        V = np.repeat(V, 2, 0)
        V = np.reshape(V, (height, width))
        # V = np.repeat(V, 2, 0)

        RGBMatrix = (np.dstack([Y, U, V])).astype(np.uint8)
        RGBMatrix = cv2.cvtColor(RGBMatrix, cv2.COLOR_YUV2RGB, 3)
        return RGBMatrix

    def RawRGBtoRGB(self, byteArray, width, height):
        image = np.array(byteArray, dtype=np.uint8)
        RGBMatrix = image.reshape(height, width, 1)
        # RGBMatrix = cv2.convertScaleAbs(RGBMatrix)

        # cv2.COLOR_BAYER_RG2RGB)#cv2.COLOR_BAYER_BG2BGR)
        RGBMatrix = cv2.cvtColor(RGBMatrix, cv2.COLOR_BAYER_GR2RGB)
        # RGBMatrix = cv2.cvtColor(RGBMatrix, cv2.COLOR_BAYER_BGR2RGB)

        return RGBMatrix

    def show_image(self, img_data, width, height, mode):
        if img_data == []:
            self.logger.warning('no valid img_data')
            return

        if mode == self.CAMERA_MODE_YUV422:
            if len(img_data) != width*height*2:
                self.logger.warning(
                    'img_data length error: '+str(len(img_data)))
                return
            # img_data.reshape(img_data, 1)
            img_np = self.YUVtoRGB(img_data, width, height)

            cv2.namedWindow('YUV', cv2.WINDOW_NORMAL)
            cv2.imshow('YUV', img_np)
            cv2.waitKey(self.img_refresh)

        elif mode == self.CAMERA_MODE_RAW_DATA:
            if len(img_data) != width * height:
                self.logger.warning(
                    'img_data length error: '+str(len(img_data)))
                return
            img_np = self.RawRGBtoRGB(img_data, width, height)

            cv2.namedWindow('RawRGB', cv2.WINDOW_NORMAL)
            cv2.imshow('RawRGB', img_np)
            cv2.waitKey(self.img_refresh)

        else:
            self.logger.warning("Can't support this mode: " + str(mode))

    def set_image(self, img_data, width, height, mode, refresh):
        self.img_data = img_data
        self.img_wd = width
        self.img_ht = height
        self.img_md = mode
        self.img_vd = True
        self.img_refresh = refresh

    def run(self):
        while True:
            time.sleep(0.1)
            if self.img_vd:
                self.show_image(self.img_data, self.img_wd,
                                self.img_ht, self.img_md)
                self.img_vd = False


class ReceiverThread():
    FRAME_FLAG = b'\x7E'
    FRAME_ESCAP = b'\x7D'
    FRAME_DIM = b'\x00'
    FRAME_END = b'\x01'

    serial_port = 0
    baud_rate = 0
    port_handler = 0

    img_data = []
    img_wd = 0
    img_ht = 0
    img_md = 0
    img_vd = False
    img_refresh = 10

    def __init__(self, port, bps, parent=None):
        self.parent = parent
        self.serial_port = port
        self.baud_rate = bps
        self.logger = logging.getLogger('image_show')
        self.port_handler = serial.Serial(self.serial_port, self.baud_rate)
        super(ReceiverThread, self).__init__()

    def validate_packet(self, byte_array):
        if len(byte_array) < 2:
            return False

        checksum = 0
        for i in range(len(byte_array)-1):
            checksum += byte_array[i]
        checksum %= 256
        if checksum == byte_array[len(byte_array)-1]:
            return True
        else:
            return False

    def parse_packet(self, byte_array):
        ret_array = []
        max_len = len(byte_array)

        if max_len <= 3:
            self.logger.warning("too short frame")
            return ret_array

        if byte_array[0] != int('0x7E', 16) or byte_array[max_len - 1] != int('0x7E', 16):
            self.logger.warning("wrong start or end")
            self.logger.warning(
                ' '.join('{:02x}'.format(x) for x in byte_array))
            return ret_array

        pos = 1
        while pos < max_len - 1:
            if byte_array[pos] == int('0x7E', 16):
                self.logger.warning("wrong intermediate data")
            if byte_array[pos] == int('0x7D', 16):
                pos += 1
                ret_array.append(byte_array[pos] ^ int('0x20', 16))
            else:
                ret_array.append(byte_array[pos])

            pos += 1

        if not self.validate_packet(ret_array):
            self.logger.warning("Checksum error!")

        return ret_array[:-1]

    def parse_dimension(self, byte_array):
        results = self.parse_packet(byte_array)
        width = int(results[0]) + int(results[1]) * 256
        height = int(results[2]) + int(results[3]) * 256
        mode = int(results[4])

        return width, height, mode

    def decode_frame(self, frm_buf):
        ret_array = []
        max_len = len(frm_buf)

        packet_start_pos = frm_buf.find(b'\x7E')
        if packet_start_pos < 0:
            self.logger.warning("Can't find the first start flag!")
            return
        packet_end_pos = frm_buf[packet_start_pos+1:].find(b'\x7E')
        if packet_end_pos < 0:
            self.logger.warning("Can't find the first end flag!")
            return
        packet_end_pos += (packet_start_pos+1)

        while packet_start_pos < max_len and packet_end_pos < max_len:
            if packet_end_pos <= packet_start_pos:
                self.logger.warning("Can't find a matched start - end flag!")
                break
            packet = frm_buf[packet_start_pos:packet_end_pos+1]
            ret_array += self.parse_packet(packet)

            packet_start_pos = frm_buf[packet_end_pos+1:].find(b'\x7E')
            if packet_start_pos < 0:
                break
            packet_start_pos += (packet_end_pos+1)

            packet_end_pos = frm_buf[packet_start_pos+1:].find(b'\x7E')
            if packet_end_pos < 0:
                self.logger.warning("Can't find a matched end flag!")
                break
            packet_end_pos += (packet_start_pos+1)

        return ret_array

    def run(self):
        if not self.port_handler.isOpen():
            self.logger.error("Open serial port first!")
        else:
            frame_buf = b''
            width = 0
            height = 0
            mode = 0
            image_data = []
            ser = self.port_handler

            refresh_time_prev = datetime.datetime.now()
            refresh_time_curr = refresh_time_prev

            while True:
                if ser.in_waiting:
                    byte_array = ser.read(ser.in_waiting)
                    frame_buf += byte_array
                    frame_end_pos = frame_buf.find(b'\x7E\x01\x7E')

                    # process if the frame end found
                    if frame_end_pos >= 0:
                        # update refresh rate
                        refresh_time_curr = datetime.datetime.now()
                        self.img_refresh = int(
                            ((refresh_time_curr - refresh_time_prev).seconds)*900) + 1
                        refresh_time_prev = refresh_time_curr

                        frame_dim_pos = frame_buf.find(b'\x7E\x00\x7E')
                        if frame_dim_pos and frame_end_pos > (frame_dim_pos+3):
                            # get the dimension info
                            width, height, mode = self.parse_dimension(
                                frame_buf[frame_dim_pos+3:frame_end_pos])
                            # process the current frame
                            image_data = self.decode_frame(
                                frame_buf[:frame_dim_pos])
                            # show the image
                            if not self.img_vd:
                                self.img_wd = width
                                self.img_ht = height
                                self.img_data = image_data
                                self.img_md = mode
                                self.img_vd = True
                        else:
                            self.logger.warning(
                                "Can't find a valid dimension info")

                        # truncate the left bytes into the next frame
                        frame_buf = frame_buf[frame_end_pos+3:]


def main(argv):
    # modify the port and baud rate accordingly
    serial_port = "/dev/ttyACM0"
    baud_rate = 921600
    try:
        opts, args = getopt.getopt(argv, "hd:b:", ["device=", "baudrate="])
    except getopt.GetoptError:
        print(
            "{0}{1} -d <serail_prot device> -b <baud_rate>".format(argv[0], arg[1]))
        sys.exit(2)
    for opt, arg in opts:
        if opt == '-h':
            print("show_image.py -d <serail_prot device> -b <baud_rate>")
            print("default Config SeailPort:{0}, Baudrate:{1}".format(
                serial_port, baud_rate))
            sys.exit()
        elif opt in ("-d", "--device"):
            serial_port = arg
        elif opt in ("-b", "--baudrate"):
            baud_rate = int(arg)
    print("Config SeailPort:{0}, Baudrate:{1}".format(serial_port, baud_rate))

    serial_dev = ReceiverThread(serial_port, baud_rate)
    receive_thrd = threading.Thread(target=serial_dev.run, daemon=True)
    receive_thrd.start()

    image_handler = ShowImage()
    print("Runing...")
    while True:
        time.sleep(0.001)
        if serial_dev.img_vd:
            image_handler.set_image(serial_dev.img_data, serial_dev.img_wd,
                                    serial_dev.img_ht, serial_dev.img_md, serial_dev.img_refresh)
            image_handler.show_image(
                serial_dev.img_data, serial_dev.img_wd, serial_dev.img_ht, serial_dev.img_md)
            serial_dev.img_vd = False


if __name__ == '__main__':
    main(sys.argv[1:])
