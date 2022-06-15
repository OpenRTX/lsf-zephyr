import pytest
from btest import device
from btest.shell import *
from btest.utils import logfile
from ctypes import *
import platform
from time import sleep
from usb2xxx.usb_device import *
from usb2xxx.usb2spi import *

@pytest.fixture
def connected_device():
    # set path of device-map.yml accordingly
    devices = device.load_devices('device-map.yml')
    assert len(devices) > 0
    with logfile('./logs', __name__) as f:
        shell = shell_open(devices[0]['shell'], log_to=f)
        yield (shell)
        shell.close()

def test_spi(connected_device):
    shell = connected_device
    assert shell.match('Booting Zephyr OS build', full_match=False, timeout=10) is not None

    DevIndex = 0
    DevHandles = (c_uint * 20)()

    # Scan device
    ret = USB_ScanDevice(byref(DevHandles))
    assert ret != 0

    # Open device
    ret = USB_OpenDevice(DevHandles[DevIndex])

    SPIConfig = SPI_CONFIG()
    SPIConfig.Mode = SPI_MODE_HARD_FDX
    SPIConfig.Master = SPI_SLAVE
    SPIConfig.CPOL = 0
    SPIConfig.CPHA = 0
    SPIConfig.LSBFirst = SPI_LSB
    SPIConfig.SelPolarity = SPI_SEL_LOW
    SPIConfig.ClockSpeedHz = 10000000
    ret = SPI_Init(DevHandles[DevIndex],SPI1_CS0,byref(SPIConfig))

    # assert ret == SPI_SUCCESS

    ReadBuffer = (c_ubyte * 255)()

    assert shell.match('spi master sending data ...', full_match=False, timeout=10) is not None
    # SPI read data
    ret = SPI_ReadBytes(DevHandles[DevIndex], SPI1_CS0, byref(ReadBuffer),len(ReadBuffer))
    assert ret == SPI_SUCCESS

    for i in range(0, len(ReadBuffer)):
        print("%02X "% ReadBuffer[i], end='')
        assert i == ReadBuffer[i]
    print("")

    # Close device
    ret = USB_CloseDevice(DevHandles[DevIndex])
    # assert bool(ret) is not False
    assert True

