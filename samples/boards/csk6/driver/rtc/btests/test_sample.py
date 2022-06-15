import pytest
from btest import device
from btest.shell import *
from btest.utils import logfile

@pytest.fixture(params=[{'logpath': './logs', 'devmap': './device-map.yml'}])
def config(request):
    return request.param

@pytest.fixture(autouse=True)
def connected_device():
    devmap = param['devmap']
    logpath = param['logpath']
    devices = device.load_devices(devmap)
    assert len(devices) > 0
    with logfile(logpath, __name__) as f:
        shell = shell_open(devices[0]['shell'], log_to=f)
        yield (shell)
        shell.close()

def test_case_1(connected_device):
    # Put your test case here
    # all filename should be started with "test_" (e.g. test_sample.py)
    shell = connected_device
    assert shell.match('Booting Zephyr OS build', full_match=False, timeout=10) is not None
    assert shell.match('RTC device sample start', full_match=False, timeout=3) is not None
    assert shell.match('RTC device alarm sample start', full_match=False, timeout=10) is not None
    assert shell.match('RTC device alarm sample end', full_match=False, timeout=10) is not None
    assert shell.match('RTC device sample end', full_match=False, timeout=10) is not None
