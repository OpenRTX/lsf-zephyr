import pytest
from btest.conftest import connected_device,reset

class Test_wifi_mgr:
    @pytest.mark.usefixtures("reset")
    def test_wifi_mgr_sample(self, connected_device):
        (shell) = connected_device
        assert shell.match(("WiFi scan done"), timeout = 10) is not None
        assert shell.match(("Connected to AP successfully"), timeout = 10) is not None