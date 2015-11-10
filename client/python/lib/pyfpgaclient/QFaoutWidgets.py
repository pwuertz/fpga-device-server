# coding:utf-8
"""

"""

from PyQt5 import QtWidgets, QtCore, QtGui

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
        bn_clear = QtWidgets.QPushButton("Clear")
        bn_clear.clicked.connect(lambda: device.sdram_clear())
        bn_upload = QtWidgets.QPushButton("Upload")
        def upload():
            _, ram_wr_ptr1 = device.sdram_rd_wr_ptr()
            #data = np.load("testsequence.npy")
            #seq_data = data.tolist()
            seq_data = [
                1<<15, 1,              # write delay=1
                1<<0 | 1<<2, 11, 12,   # write regs 0 and 2
                1<<14, 1<<3,           # write hold cmd
                1<<4 | 1<<6, 13, 14,   # write regs 4 and 6
                1<<15, 0               # write end
            ]
            device.sdram_write(seq_data)
            _, ram_wr_ptr2 = device.sdram_rd_wr_ptr()
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
        layout_bns.addWidget(bn_reset, 1, 2)


class Sliders(QtWidgets.QWidget):

    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QGridLayout(self)

        def gen_write_func(device, ch):
            def write_func(val):
                device.write_voltage(ch, val)
            return write_func

        self.sliders = []
        for i in range(6):
            slider = QtWidgets.QSlider()
            slider.setRange(-10, 10)
            slider.valueChanged.connect(gen_write_func(device, i))
            self.sliders.append(slider)
            layout.addWidget(slider, i, 0)

            edit = QtWidgets.QLineEdit()
            edit.setValidator(QtGui.QDoubleValidator())
            edit.validator().setRange(-10, 10, 3)
            edit.textChanged.connect(slider.setValue)
            layout.addWidget(edit, i , 1)

        for i in range(6):
            v = device.read_voltage(i)
            self.sliders[i].setValue(v)


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
        device.client.statusChanged.connect(self.handleStatus)

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


class DeviceWidget(QtWidgets.QWidget):
    def __init__(self, device, shell):
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
