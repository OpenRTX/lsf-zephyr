# Copyright (c) 2019-2022 Anhui LISTENAI Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner for flashing CSK chips with cskburn.'''

from runners.core import ZephyrBinaryRunner, RunnerCaps, BuildConfiguration
from enquiries import choose
from serial.tools import list_ports


class CskBinaryRunner(ZephyrBinaryRunner):
    '''Runner front-end for cskburn.'''

    def __init__(self, cfg,  port, baud=None, chip='6', addr=0):
        super().__init__(cfg)
        self.port = port
        self.baud = baud
        self.chip = chip
        self.addr = addr
        self.bin_file = cfg.bin_file

    @classmethod
    def name(cls):
        return 'csk'

    @classmethod
    def capabilities(cls):
        return RunnerCaps(commands={'flash'}, flash_addr=True)

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--port',
                            help='serial port to flash')
        parser.add_argument('--baud', default='748800',
                            help='serial baud rate')
        parser.add_argument('--chip', default='6',
                            help='family of chip')
        parser.add_argument('--addr',
                            help='override address to flash')

    @classmethod
    def do_create(cls, cfg, args):
        build_conf = BuildConfiguration(cfg.build_dir)
        if args.addr:
            flash_addr = int(args.addr, 0)
        elif args.dt_flash and build_conf.getboolean('CONFIG_HAS_FLASH_LOAD_OFFSET'):
            flash_addr = build_conf['CONFIG_FLASH_LOAD_OFFSET']
        else:
            flash_addr = 0

        if args.port == None:
            ports = [p.device for p in list_ports.comports()]
            args.port = choose('Choose a port to flash', ports)

        return CskBinaryRunner(
            cfg, chip=args.chip, port=args.port,  baud=args.baud,
            addr=flash_addr)

    def do_run(self, command, **kwargs):
        cmd_flash = ['cskburn']
        cmd_flash.extend(['--chip', self.chip])
        cmd_flash.extend(['--serial', self.port])
        if self.baud is not None:
            cmd_flash.extend(['--baud', self.baud])
        cmd_flash.extend(['--reset-delay', '1'])
        cmd_flash.extend([hex(self.addr), self.bin_file])

        self.logger.info("Flashing CSK chip on {} ({}bps)".format(
            self.port, self.baud))
        self.check_call(cmd_flash)
