import sys
import getopt

import argparse
import os
import re

import binascii
import struct

flash_map = {
    'ap':{
    'name':'ap/zephyr.bin',
    'location': 0x18000000,
    },
    'littlefs':{
    'name':'cp/lfs1.bin',
    'location': 0x18260000,   
    },
    'cp':{
    'name':'cp/cp.bin',
    'location': 0x18500000,   
    },
    
    'cutline':{
    'name':'algo_resource/mlp_cutline_castor.bin',
    'location': 0x18800000,
    },
    'ocr':{
    'name':'algo_resource/venus_ocr_ctc_v5.bin',
    'location': 0x18880000,   
    },
    'tts':{
    'name':'algo_resource/ivTTS_CE+PE_Castor_xiaoyan_v3.16K.bin',
    'location': 0x18b00000,   
    },
    'translate':{
    'name':'algo_resource/translate.bin',
    'location': 0x18f80000,   
    },
}

def read_config_and_merge(fo):

    data = bytearray()
    
    offset = 0
    skip = 0
    size = 0
    
    # 
    for key,val in flash_map.items():
        
        f = open(val['name'], 'rb')
        
        # fill zero
        offset = val['location'] - 0x18000000         # addr
        skip = offset - size        
        print('[offset]: ' + '0x{0:02x}'.format(offset) + ' [size]: ' + '0x{0:02x}'.format(size) + ' [skip]: ' + '0x{0:02x}'.format(skip) )
        #data.zfill(skip)                             # can't use
        data += bytearray(skip)
        
        # size        
        stat = os.stat(val['name'])
        size = stat.st_size
        
        # update size
        size += offset
        #print("[size]: " + '0x{0:02x}'.format(size))
        
        # fill bin        
        data += f.read()
        f.close()
        
    # fill last zero
    skip = (32 * 1024 * 1024) - size
    data += bytearray(skip)
    
    # output
    f = open(fo, 'wb')
    f.write(data)
    f.close()
    
    return
   
  

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument('-i', type=str, help='请输入文件名')
    parser.add_argument('-o', type=str, help='请输入文件名')
    args = parser.parse_args()
    
    # need check
    
    return args

def main():
    # argv parse
    arg = parse_args()
        
    # make bmp header
    read_config_and_merge(arg.o)
    
    print("Bin merge Done !")
    

if __name__ == '__main__':
    main()
