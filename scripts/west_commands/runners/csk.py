# Copyright (c) 2019-2022 Anhui LISTENAI Co., Ltd.
#
# SPDX-License-Identifier: Apache-2.0

'''Runner for flashing CSK chips with cskburn.'''

from runners.core import ZephyrBinaryRunner, RunnerCaps, BuildConfiguration


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
        return RunnerCaps(commands={'flash'})

    @classmethod
    def do_add_parser(cls, parser):
        parser.add_argument('--port', required=True,
                            help='serial port to flash')
        parser.add_argument('--baud', default='748800',
                            help='serial baud rate')
        parser.add_argument('--chip', default='6',
                            help='family of chip')

    @classmethod
    def do_create(cls, cfg, args):
        build_conf = BuildConfiguration(cfg.build_dir)
        flash_addr = cls.get_flash_address(args, build_conf)

        return CskBinaryRunner(
            cfg, chip=args.chip, port=args.port,  baud=args.baud,
            addr=flash_addr)

    def do_run(self, command, **kwargs):
        cmd_flash = ['cskburn']
        cmd_flash.extend(['--chip', self.chip])
        cmd_flash.extend(['--serial', self.port])
        if self.baud is not None:
            cmd_flash.extend(['--baud', self.baud])
        cmd_flash.extend(['--reset-nanokit'])
        cmd_flash.extend([hex(self.addr), self.bin_file])

        self.logger.info("Flashing CSK chip on {} ({}bps)".format(
            self.port, self.baud))
        self.check_call(cmd_flash)
