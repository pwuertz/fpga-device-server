from PyQt5 import QtCore, QtGui, QtWidgets
from QFaoutClient import QFaoutClient
import numpy as np

class Controls(QtWidgets.QWidget):
    def __init__(self):
        QtWidgets.QWidget.__init__(self)
        sl1 = QtWidgets.QSlider()
        sl2 = QtWidgets.QSlider()
        sl1.setRange(0, 1000)
        sl2.setRange(0, 1000)

        layout = QtWidgets.QVBoxLayout(self)

        layout_sl = QtWidgets.QHBoxLayout()
        layout_sl.addWidget(sl1)
        layout_sl.addWidget(sl2)
        sl1.valueChanged.connect(lambda v: faout.write_dac1(serial, int(0xffff * (v/1000.) )))
        sl2.valueChanged.connect(lambda v: faout.write_dac2(serial, int(0xffff * (v/1000.) )))
        sl1.valueChanged.connect(lambda v: faout.write_display(serial, int(0xff * (v/1000.) )))
        sl2.valueChanged.connect(lambda v: faout.write_display(serial, int(0xff * (v/1000.) )))
        layout.addLayout(layout_sl)

        bn_start = QtWidgets.QPushButton("Start")
        bn_start.clicked.connect(lambda: faout.sequence_start(serial))
        bn_stop = QtWidgets.QPushButton("Stop")
        bn_stop.clicked.connect(lambda: faout.sequence_stop(serial))
        bn_reset = QtWidgets.QPushButton("Reset")
        bn_reset.clicked.connect(lambda: faout.sequence_reset(serial))
        bn_resetclock = QtWidgets.QPushButton("Reset Ext Clock")
        bn_resetclock.clicked.connect(lambda: faout.write_reg(serial, 0, 0x1 << 3))
        bn_upload = QtWidgets.QPushButton("Upload")
        def upload():
            ram_wr_ptr1 = faout.get_ram_write_ptr(serial)
            data = np.load("testsequence.npy")
            ldata = data.tolist()
            faout.write_ram(serial, ldata)
            ram_wr_ptr2 = faout.get_ram_write_ptr(serial)
            infostr = "Write position before: 0x%x\n" % ram_wr_ptr1
            infostr += "Write position after: 0x%x\n" % ram_wr_ptr2
            QtWidgets.QMessageBox.information(self, "DRAM Info", infostr)
        bn_upload.clicked.connect(upload)

        layout.addWidget(bn_start)
        layout.addWidget(bn_stop)
        layout.addWidget(bn_reset)
        layout.addWidget(bn_upload)
        layout.addWidget(bn_resetclock)


class Status(QtWidgets.QWidget):
    def __init__(self):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QFormLayout(self)

        # build layout for status
        status_dict = faout.get_device_status(serial)
        self.status_labels = {}
        for k, v in status_dict.iteritems():
            label = QtWidgets.QLabel(str(v))
            layout.addRow(k, label)
            self.status_labels[k] = label

        faout.statusUpdate.connect(self.handleStatus)

    def handleStatus(self, serial, status_dict):
        for k, v in status_dict.iteritems():
            self.status_labels[k].setText(str(v))

class Main(QtWidgets.QWidget):
    def __init__(self):
        QtWidgets.QWidget.__init__(self)
        layout = QtWidgets.QHBoxLayout(self)
        layout.addWidget(Controls())
        layout.addWidget(Status())
        self.resize(200, 400)


app = QtWidgets.QApplication([])

faout = QFaoutClient("localhost", 9001)
serial = faout.get_device_list()[0]
print("Connected to server, using device %s" % serial)
print("Firmware version: %d" % faout.read_reg(serial, reg=10))

win = Main()
win.show()
app.exec_()
