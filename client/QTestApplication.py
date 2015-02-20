import inspect
import numpy as np

from PyQt5 import QtCore, QtGui, QtWidgets
from IPython.qt.console.rich_ipython_widget import RichIPythonWidget, IPythonWidget
from IPython.qt.inprocess import QtInProcessKernelManager
from QFaoutClient import QFaoutClient


class Controls(QtWidgets.QWidget):
    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)

        # sliders for controlling the outputs
        self.slider_widget = Sliders(device)

        # buttons for commands
        bn_arm = QtWidgets.QPushButton("Arm")
        bn_arm.clicked.connect(lambda: device.sequence_arm())
        bn_start = QtWidgets.QPushButton("Start")
        bn_start.clicked.connect(lambda: device.sequence_start())
        bn_stop = QtWidgets.QPushButton("Stop")
        bn_stop.clicked.connect(lambda: device.sequence_stop())
        bn_hold = QtWidgets.QPushButton("Hold")
        bn_hold.clicked.connect(lambda: device.sequence_hold())
        bn_reset = QtWidgets.QPushButton("Reset")
        bn_reset.clicked.connect(lambda: device.reset())
        bn_resetclock = QtWidgets.QPushButton("Reset ClkExt")
        bn_resetclock.clicked.connect(lambda: device.write_reg(0, 0x1 << 5))
        bn_clear = QtWidgets.QPushButton("Clear")
        bn_clear.clicked.connect(lambda: device.sequence_clear())
        bn_upload = QtWidgets.QPushButton("Upload")
        def upload():
            ram_wr_ptr1 = device.get_ram_write_ptr()
            #data = np.load("testsequence.npy")
            #seq_data = data.tolist()
            seq_data = [
                1<<15, 1,              # write delay=1
                1<<0 | 1<<2, 11, 12,   # write regs 0 and 2
                1<<14, 1<<3,           # write hold cmd
                1<<4 | 1<<6, 13, 14,   # write regs 4 and 6
                1<<15, 0               # write end
            ]
            device.write_ram(seq_data)
            ram_wr_ptr2 = device.get_ram_write_ptr()
            infostr = "Write position before: 0x%x\n" % ram_wr_ptr1
            infostr += "Write position after: 0x%x\n" % ram_wr_ptr2
            QtWidgets.QMessageBox.information(self, "DRAM Info", infostr)
        bn_upload.clicked.connect(upload)

        # build layout
        layout = QtWidgets.QVBoxLayout(self)
        layout.addWidget(self.slider_widget)

        layout_bns = QtWidgets.QGridLayout()
        layout.addLayout(layout_bns)
        layout_bns.addWidget(bn_arm, 0, 0)
        layout_bns.addWidget(bn_start, 0, 1)
        layout_bns.addWidget(bn_stop, 0, 2)
        layout_bns.addWidget(bn_hold, 0, 3)
        layout_bns.addWidget(bn_upload, 1, 0)
        layout_bns.addWidget(bn_clear, 1, 1)
        layout_bns.addWidget(bn_resetclock, 1, 2)
        layout_bns.addWidget(bn_reset, 1, 3)


class Sliders(QtWidgets.QWidget):

    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QHBoxLayout(self)

        def gen_write_func(device, ch):
            def write_func(val):
                val = int(0xffff * (val/1000.))
                device.write_dac(ch, val)
            return write_func

        self.sliders = []
        for i in range(6):
            slider = QtWidgets.QSlider()
            slider.setRange(0, 1000)
            slider.valueChanged.connect(gen_write_func(device, i))
            self.sliders.append(slider)
            layout.addWidget(slider)

        for i in range(6):
            v = device.read_dac(i)
            self.sliders[i].setValue(int(v * (1./0xffff) * 1000.))


class Status(QtWidgets.QWidget):
    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QFormLayout(self)

        # build layout for status
        status_dict = device.get_device_status()
        self.status_labels = {}
        for k, v in status_dict.iteritems():
            label = QtWidgets.QLabel(str(v))
            layout.addRow(k, label)
            self.status_labels[k] = label

        layout.addRow("Firmware", QtWidgets.QLabel(str(device.get_version())))

        self.device = device
        device.client.statusUpdate.connect(self.handleStatus)

    def handleStatus(self, serial, status_dict):
        if self.device.serial == serial:
            for k, v in status_dict.iteritems():
                self.status_labels[k].setText(str(v))


class Config(QtWidgets.QWidget):
    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QFormLayout(self)

        # build layout for config register
        cb_clk_ext = QtWidgets.QCheckBox()
        cb_clk_ext.setChecked(device.get_clock_extern())
        cb_clk_ext.toggled.connect(lambda enabled: device.set_clock_extern(enabled))

        layout.addRow("Clock Ext", cb_clk_ext)


class DeviceMeta(type):
    def __new__(cls, name, parents, dct):
        def func_wrapper_gen(func):
            return lambda self, *args: func(self.client, self.serial, *args)

        for (n, o) in inspect.getmembers(QFaoutClient, inspect.ismethod):
            args = inspect.getargspec(o).args
            if len(args) >= 2 and args[1] == "serial":
                dct[n] = func_wrapper_gen(o)
                dct[n].__doc__ = o.__doc__
                dct[n].__name__ = o.__name__
        return super(DeviceMeta, cls).__new__(cls, name, parents, dct)


class Device(object):
    __metaclass__ = DeviceMeta

    def __init__(self, client, serial):
        self.client = client
        self.serial = serial

    def __str__(self):
        return self.serial

    def __repr__(self):
        return "<Device: %s>" % self.serial


class DeviceWidget(QtWidgets.QWidget):
    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QHBoxLayout(self)

        def addBoxedWidget(title, widget):
            box = QtWidgets.QGroupBox(title)
            box_layout = QtWidgets.QVBoxLayout(box)
            box_layout.addWidget(widget)
            layout.addWidget(box)

        addBoxedWidget("Controls", Controls(device))
        addBoxedWidget("Status Register", Status(device))
        addBoxedWidget("Config Register", Config(device))


class Main(QtWidgets.QWidget):
    def __init__(self, client):
        QtWidgets.QWidget.__init__(self)
        self.client = client
        self.devices = []

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
        self._console = IPythonWidget(font_size=9)
        self._console.kernel_manager = kernel_manager
        self._console.kernel_client = kernel_client
        layout.addWidget(self._console)

        # add devices that are already present and handlers for hotplugging
        for serial in client.get_device_list():
            self.addDevice(serial)
        client.addedDevice.connect(self.addDevice)
        client.removedDevice.connect(self.removeDevice)

        # push client and device list to console namespace
        dct = {"client": client, "devices": self.devices}
        kernel_manager.kernel.shell.push(dct)

        self.resize(300, 600)
        self.setWindowTitle("FAout Test Application")
    
    def addDevice(self, serial):
        device = Device(self.client, serial)
        self.devices.append(device)
        device_widget = DeviceWidget(device)
        self.device_tabs.addTab(device_widget, serial)

    def removeDevice(self, serial):
        for i in range(self.device_tabs.count()):
            if self.device_tabs.tabText(i) == serial:
                del self.devices[i]
                self.device_tabs.removeTab(i)
                return

    def onCurrentDeviceChanged(self, index):
        if index >= 0:
            dct = {"dev": self.devices[index]}
        else:
            dct = {"dev": None}
        self._console.kernel_manager.kernel.shell.push(dct)


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description='FAout Test Application')
    parser.add_argument('host', nargs='?', default='localhost')
    parser.add_argument('port', nargs='?', type=int, default=9001)

    args = parser.parse_args()
    app = QtWidgets.QApplication([])
    client = QFaoutClient(args.host, args.port)
    win = Main(client)
    win.show()
    app.exec_()
