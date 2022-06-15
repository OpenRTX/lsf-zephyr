import os
import pytest
from btest import device
from btest.shell import *
from btest.utils import logfile

@pytest.fixture
def connected_device():
    devmap_path = os.environ.get('LISA_RUNNER_DEVMAP_PATH', './device-map.yml')
    devices = device.load_devices(devmap_path)
    assert len(devices) > 0
    with logfile('./logs', __name__) as f:
        shell = shell_open(devices[0]['shell'], log_to=f)
        yield (shell)
        shell.close()


def test_help(connected_device):
    # Put your test case here
    (shell) = connected_device
    assert shell.match('Hello World!') is not None
