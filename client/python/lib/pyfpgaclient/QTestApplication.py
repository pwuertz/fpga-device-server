# -*- coding: utf-8 -*-
#-----------------------------------------------------------------------------
# Author: Peter Würtz, TU Kaiserslautern (2016)
#
# Distributed under the terms of the GNU General Public License Version 3.
# The full license is in the file COPYING.txt, distributed with this software.
#-----------------------------------------------------------------------------

import inspect
import numpy as np

from PyQt5 import QtCore, QtGui, QtWidgets
from qtconsole.rich_jupyter_widget import RichJupyterWidget as RichIPythonWidget, JupyterWidget as IPythonWidget
from qtconsole.inprocess import QtInProcessKernelManager
from IPython.display import display

from pyfpgaclient.QFpgaClient import QFpgaClient
from pyfpgaclient.QFaoutWidgets import DeviceWidget as FAoutDeviceWidget


class QDeviceWidget(QtWidgets.QWidget):
    def __init__(self, device, shell):
        QtWidgets.QWidget.__init__(self)
        self.device = device
        layout = QtWidgets.QVBoxLayout(self)

        # create buttons for methods that do not require arguments
        methods = inspect.getmembers(device, predicate=inspect.ismethod)
        methods = [(n, m) for (n, m) in methods if not n.startswith("_")]
        layout_methods = QtWidgets.QHBoxLayout()
        layout.addLayout(layout_methods)
        for (n, m) in methods:
            method_args = inspect.getargspec(m).args
            if len(method_args) > 2:
                continue
            if len(method_args) == 2 and not method_args[1].startswith("enable"):
                continue
            checkable = len(method_args) == 2
            btn = QtWidgets.QPushButton(n)
            btn.setCheckable(checkable)
            def wrap_gen(n, m, checkable):
                def callfunc(checked):
                    result = m() if not checkable else m(checked)
                    msg = "dev.%s(%s): %s" % (n, checked if checkable else "", result)
                    display(msg)
                return callfunc
            btn.clicked.connect(wrap_gen(n, m, checkable))
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


def run(host='localhost', port=9002):
    import argparse
    parser = argparse.ArgumentParser(description='Fpga Test Application')
    parser.add_argument('host', nargs='?', default=host)
    parser.add_argument('port', nargs='?', type=int, default=port)

    args = parser.parse_args()
    app = QtWidgets.QApplication([])
    client = QFpgaClient(args.host, args.port)
    QTestApplication.DEVICE_WIDGET_MAP['FAOUT'] = FAoutDeviceWidget
    win = QTestApplication(client)
    win.show()
    app.exec_()

if __name__ == "__main__":
    run()
