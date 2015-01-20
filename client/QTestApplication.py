from PyQt5 import QtCore, QtGui, QtWidgets
from QFaoutClient import QFaoutClient
import numpy as np

class Controls(QtWidgets.QWidget):
    def __init__(self):
        QtWidgets.QWidget.__init__(self)

        def gen_write_func(ch):        
            def write_func(val):
                val = int(0xffff * (val/1000.))
                print("Writing ch%d: %x" % (ch, val))
                client.write_dac(serial, ch, val)
            return write_func

        layout = QtWidgets.QVBoxLayout(self)

        layout_sl = QtWidgets.QHBoxLayout()
        self.sliders = []
        for i in range(6):
            slider = QtWidgets.QSlider()
            slider.setRange(0, 1000)
            slider.valueChanged.connect(gen_write_func(i))
            self.sliders.append(slider)
            layout_sl.addWidget(slider)

        layout.addLayout(layout_sl)

        bn_start = QtWidgets.QPushButton("Start")
        bn_start.clicked.connect(lambda: client.sequence_start(serial))
        bn_stop = QtWidgets.QPushButton("Stop")
        bn_stop.clicked.connect(lambda: client.sequence_stop(serial))
        bn_reset = QtWidgets.QPushButton("Reset")
        bn_reset.clicked.connect(lambda: client.sequence_reset(serial))
        bn_resetclock = QtWidgets.QPushButton("Reset Ext Clock")
        bn_resetclock.clicked.connect(lambda: client.write_reg(serial, 0, 0x1 << 3))
        bn_upload = QtWidgets.QPushButton("Upload")
        def upload():
            ram_wr_ptr1 = client.get_ram_write_ptr(serial)
            data = np.load("testsequence.npy")
            ldata = data.tolist()
            client.write_ram(serial, ldata)
            ram_wr_ptr2 = client.get_ram_write_ptr(serial)
            infostr = "Write position before: 0x%x\n" % ram_wr_ptr1
            infostr += "Write position after: 0x%x\n" % ram_wr_ptr2
            QtWidgets.QMessageBox.information(self, "DRAM Info", infostr)
        bn_upload.clicked.connect(upload)

        def get_values():
            for i in range(6):
                v = client.read_dac(serial, i)
                self.sliders[i].setValue(int(v * (1./0xffff) * 1000.))
        get_values()

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
        status_dict = client.get_device_status(serial)
        self.status_labels = {}
        for k, v in status_dict.iteritems():
            label = QtWidgets.QLabel(str(v))
            layout.addRow(k, label)
            self.status_labels[k] = label

        client.statusUpdate.connect(self.handleStatus)

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
        self.setWindowTitle("FAout Test App")


if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser(description='Faout-Client Example')
    parser.add_argument('host', nargs='?', default='localhost')
    parser.add_argument('port', nargs='?', type=int, default=9001)
    args = parser.parse_args()

    app = QtWidgets.QApplication([])

    client = QFaoutClient(args.host, args.port)
    print 'Connected to %s' % args.host
    devices = client.get_device_list()
    assert devices, "No devices connected"
    print("Connected devices: %s" % (", ".join(devices)))
    serial = client.get_device_list()[0]
    print("Using device %s" % serial)
    print("Firmware version: %d" % client.get_version(serial))

    win = Main()
    win.show()
    app.exec_()

