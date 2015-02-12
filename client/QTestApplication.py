import inspect
import numpy as np

from PyQt5 import QtCore, QtGui, QtWidgets
from QFaoutClient import QFaoutClient


class Controls(QtWidgets.QWidget):
    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)

        layout = QtWidgets.QVBoxLayout(self)

        bn_arm = QtWidgets.QPushButton("Arm")
        bn_arm.clicked.connect(lambda: device.sequence_arm())
        bn_start = QtWidgets.QPushButton("Start")
        bn_start.clicked.connect(lambda: device.sequence_start())
        bn_stop = QtWidgets.QPushButton("Stop")
        bn_stop.clicked.connect(lambda: device.sequence_stop())
        bn_reset = QtWidgets.QPushButton("Reset")
        bn_reset.clicked.connect(lambda: device.sequence_reset())
        bn_resetclock = QtWidgets.QPushButton("Reset Ext Clock")
        bn_resetclock.clicked.connect(lambda: device.write_reg(0, 0x1 << 3))
        bn_upload = QtWidgets.QPushButton("Upload")
        def upload():
            ram_wr_ptr1 = device.get_ram_write_ptr()
            data = np.load("testsequence.npy")
            ldata = data.tolist()
            device.write_ram(ldata)
            ram_wr_ptr2 = device.get_ram_write_ptr()
            infostr = "Write position before: 0x%x\n" % ram_wr_ptr1
            infostr += "Write position after: 0x%x\n" % ram_wr_ptr2
            QtWidgets.QMessageBox.information(self, "DRAM Info", infostr)
        bn_upload.clicked.connect(upload)

        self.slider_widget = Sliders(device)
        layout.addWidget(self.slider_widget)
        layout.addWidget(bn_arm)
        layout.addWidget(bn_start)
        layout.addWidget(bn_stop)
        layout.addWidget(bn_reset)
        layout.addWidget(bn_upload)
        layout.addWidget(bn_resetclock)


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
        cb_auto_arm = QtWidgets.QCheckBox()
        cb_auto_arm.setChecked(device.get_auto_arm())
        cb_auto_arm.toggled.connect(lambda enabled: device.set_auto_arm(enabled))

        layout.addRow("Clock Ext", cb_clk_ext)
        layout.addRow("Auto Arm", cb_auto_arm)


class DeviceMeta(type):
    def __new__(cls, name, parents, dct):
        def func_wrapper_gen(func):
            return lambda self, *args: func(self.client, self.serial, *args)

        for (n, o) in inspect.getmembers(QFaoutClient, inspect.ismethod):
            args = inspect.getargspec(o).args
            if len(args) >= 2 and args[1] == "serial":
                dct[n] = func_wrapper_gen(o)
        return super(DeviceMeta, cls).__new__(cls, name, parents, dct)


class Device(object):
    __metaclass__ = DeviceMeta

    def __init__(self, client, serial):
        self.client = client
        self.serial = serial


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
        
        layout = QtWidgets.QHBoxLayout(self)
        
        self.device_tabs = QtWidgets.QTabWidget()
        layout.addWidget(self.device_tabs)
        self.resize(300, 500)
        self.setWindowTitle("FAout Test Application")

        for serial in client.get_device_list():
            self.addDevice(serial)
        client.addedDevice.connect(self.addDevice)
        client.removedDevice.connect(self.removeDevice)
    
    def addDevice(self, serial):
        device = Device(self.client, serial)
        device_widget = DeviceWidget(device)
        self.device_tabs.addTab(device_widget, serial)

    def removeDevice(self, serial):
        for i in range(self.device_tabs.count()):
            if self.device_tabs.tabText(i) == serial:
                self.device_tabs.removeTab(i)
                return


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
