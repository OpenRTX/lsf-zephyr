import pytest
import os
from btest.conftest import connected_device,reset
from btest.shell import *


devmap_path = os.environ.get('LISA_RUNNER_DEVMAP_PATH', './device-map.yml')
path =[devmap_path]
@pytest.mark.parametrize('connected_device', path, indirect=True)
class Test_wifi_mgr:

    @pytest.mark.usefixtures("reset")
    def test_wifi_mgr_sample(self, connected_device):
        shell: Shell = connected_device
        assert shell.match(("WiFi scan done"), timeout = 10) is not None
        assert shell.match(("Connected to AP successfully"), timeout = 10) is not None