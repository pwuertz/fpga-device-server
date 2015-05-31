import inspect
import numpy as np

from PyQt5 import QtCore, QtGui, QtWidgets
from IPython.qt.console.rich_ipython_widget import RichIPythonWidget, IPythonWidget
from IPython.qt.inprocess import QtInProcessKernelManager
import IPython.display
from QFpgaClient import QFpgaClient


class QDeviceWidget(QtWidgets.QWidget):
    def __init__(self, device, shell):
        QtWidgets.QWidget.__init__(self)
        self.device = device
        layout = QtWidgets.QVBoxLayout(self)

        # create buttons for methods that do not require arguments
        methods = inspect.getmembers(device, predicate=inspect.ismethod)
        methods = [(n, m) for (n, m) in methods if not n.startswith("_")]
        methods_simple = [(n, m) for (n, m) in methods if len(inspect.getargspec(m).args) == 1]

        layout_methods = QtWidgets.QHBoxLayout()
        layout.addLayout(layout_methods)
        for (n, m) in methods_simple:
            btn = QtWidgets.QPushButton(n)
            def wrap_gen(n, m):
                def callfunc():
                    result = m()
                    IPython.display.display("dev.%s(): %s" % (n, result))
                return callfunc
            btn.clicked.connect(wrap_gen(n, m))
            layout_methods.addWidget(btn)

        # create label for register changed events
        self.label = QtWidgets.QLabel()
        layout.addWidget(self.label)
        def show_reg_changed(addr, port, value):
            self.label.setText("Register %d:%d changed: value=0x%04x" % (addr, port, value))
        device.registerChanged.connect(show_reg_changed)


class QTestApplication(QtWidgets.QWidget):

    DEVICE_WIDGET_MAP = {}

    def __init__(self, client):
        QtWidgets.QWidget.__init__(self)
        self.client = client
        self.devices = {}

        # build layout
        layout = QtWidgets.QVBoxLayout(self)

        # tab widget for each connected device
        self.device_tabs = QtWidgets.QTabWidget()
        self.device_tabs.currentChanged.connect(self.onCurrentDeviceChanged)
        layout.addWidget(self.device_tabs)

        # ipython console for scripting
        kernel_manager = QtInProcessKernelManager()
        kernel_manager.start_kernel()
        kernel_client = kernel_manager.client()
        kernel_client.start_channels()
        self._console = RichIPythonWidget(font_size=9)
        self._console.kernel_manager = kernel_manager
        self._console.kernel_client = kernel_client
        layout.addWidget(self._console)

        # set device added and remove callbacks and connect
        client.deviceAdded.connect(self.addDevice)
        client.deviceRemoved.connect(self.removeDevice)
        client.connect()

        # push client and device map to console namespace
        dct = {"client": client, "devices": self.devices, "console": self._console}
        kernel_manager.kernel.shell.push(dct)

        self.resize(800, 500)
        self.setWindowTitle("Fpga Test Application")
    
    def addDevice(self, serial, device):
        self.devices[serial] = device

        # find specialized widget for device serial
        for prefix, DeviceWidget in self.DEVICE_WIDGET_MAP.items():
            if serial.startswith(prefix):
                device_widget = DeviceWidget(device, self._console.kernel_manager.kernel.shell)
                break
        else:
            # no match for serial, create default widget
            device_widget = QDeviceWidget(device, self._console.kernel_manager.kernel.shell)

        self.device_tabs.addTab(device_widget, serial)

    def removeDevice(self, serial):
        for i in range(self.device_tabs.count()):
            if self.device_tabs.tabText(i) == serial:
                del self.devices[serial]
                self.device_tabs.removeTab(i)
                return

    def onCurrentDeviceChanged(self, index):
        if index >= 0:
            serial = self.device_tabs.tabText(index)
            dct = {"dev": self.devices[serial]}
        else:
            dct = {"dev": None}
        self._console.kernel_manager.kernel.shell.push(dct)


def run():
    import argparse
    parser = argparse.ArgumentParser(description='Fpga Test Application')
    parser.add_argument('host', nargs='?', default='localhost')
    parser.add_argument('port', nargs='?', type=int, default=9002)

    args = parser.parse_args()
    app = QtWidgets.QApplication([])
    client = QFpgaClient(args.host, args.port)
    win = QTestApplication(client)
    win.show()
    app.exec_()

if __name__ == "__main__":
    run()
