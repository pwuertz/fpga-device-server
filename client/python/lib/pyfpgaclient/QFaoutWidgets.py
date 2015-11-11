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


class ValueWidget(QtCore.QObject):
    def __init__(self, device, ch):
        QtCore.QObject.__init__(self)
        slider = QtWidgets.QSlider()
        slider.setRange(0, 1000)

        slider.valueChanged.connect(self._setValue)

        edit = QtWidgets.QLineEdit()
        edit.setMaxLength(6)
        edit.returnPressed.connect(lambda: self.setVoltage(float(edit.text())))

        self.device = device
        self.slider = slider
        self.edit = edit
        self.ch = ch

    def _setValue(self, v):
        voltage = float(20.e-3 * v -10.)
        self.edit.setText("%.3f" % (voltage))
        self.device.write_voltage(self.ch, voltage)

    def setVoltage(self, voltage):
        self.edit.setText("%.3f" % (voltage))
        slider_value = round((voltage+10.)/20.*1e3)
        self.slider.setValue(slider_value)


class Sliders(QtWidgets.QWidget):

    def __init__(self, device):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QGridLayout(self)

        self.values = []
        for i in range(6):
            value = ValueWidget(device, i)
            self.values.append(value)
            layout.addWidget(value.slider, 0, i)
            layout.addWidget(value.edit, 1, i)

        for i in range(6):
            v = device.read_voltage(i)
            self.values[i].setVoltage(v)


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
