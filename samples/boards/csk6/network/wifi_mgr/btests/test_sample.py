import pytest
import os
from btest import device
from btest.shell import *
from btest.utils import logfile


@pytest.fixture
def connected_device():
    # set path of device-map.yml accordingly
    devmap_path = os.environ.get('LISA_RUNNER_DEVMAP_PATH', './device-map.yml')
    devices = device.load_devices(devmap_path)
    assert len(devices) > 0
    with logfile('./logs', __name__) as f:
        shell = shell_open(devices[0]['shell'], log_to=f)
        yield (shell)
        shell.close()


def test_case_scan_ap(connected_device):
    # Put your test case here
    # all filename should be started with "test_" (e.g. test_sample.py)

    (shell) = connected_device
    assert shell.match(("WiFi scan done"), timeout = 10) is not None


def test_case_connect_ap(connected_device):
    # Put your test case here
    # all filename should be started with "test_" (e.g. test_sample.py)

    (shell) = connected_device
    assert shell.match(("Connected to AP successfully"), timeout = 10) is not None