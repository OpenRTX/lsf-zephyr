# Copyright (c) 2022 listenai
#
# SPDX-License-Identifier: Apache-2.0

'''my_west_extension.py

Basic example of a west extension.'''

from textwrap import dedent            # just for nicer code indentation
from west.commands import WestCommand  # your extension must subclass this
from west import log                   # use this for user output
import os
import time
import subprocess

def cmd_exec(cmd, cwd=None, shell=False):
    return subprocess.check_call(cmd, cwd=cwd, shell=shell)

class Upload(WestCommand):

    def __init__(self):
        super().__init__(
            'upload',  # gets stored as self.name
            'Upload all submitted code to the specified remote repository.',  # self.help
            # self.description:
            dedent('''
            Upload all submitted code to the specified remote repository.

            eg:
            west upload -r lscloud.'''))

    def do_add_parser(self, parser_adder):
        parser = parser_adder.add_parser(self.name,
                                         help=self.help,
                                         description=self.description)
        parser.add_argument('-r', '--remote', help='remote')
        return parser

    def do_run(self, args, unknown_args):
        cmd_exec(("git", "log"), cwd=os.getcwd())
