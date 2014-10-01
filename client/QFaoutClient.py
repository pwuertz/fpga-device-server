from FaoutClientBase import FaoutClientBase
from PyQt5 import QtCore, QtNetwork
from qao.io.messageBus import DEFAULT_TIMEOUT

class QFaoutClient(QtCore.QObject, FaoutClientBase):
    DEFAULT_TIMEOUT = 5

    addedDevice = QtCore.pyqtSignal(str)
    removedDevice = QtCore.pyqtSignal(str)
    statusUpdate = QtCore.pyqtSignal(str, object)
    __eventsPending = QtCore.pyqtSignal()

    def __init__(self, host, port=9000):
        QtCore.QObject.__init__(self,
                                send_data_cb=self._handle_send_data,
                                require_data_cb=self._handle_require_data,
                                events_pending_cb=lambda: self.__eventsPending.emit(),
                                )
        # always queue pending events in order to
        # prevent event processing within synchronous calls
        self.__eventsPending.connect(self._handle_events_pending, type=QtCore.Qt.QueuedConnection)

        self.__socket = QtNetwork.QTcpSocket()
        self.__socket.readyRead.connect(self._handle_ready_read)
        self.__socket.connectToHost(host, port)
        self.__socket.waitForConnected(DEFAULT_TIMEOUT * 1000)

    def _handle_ready_read(self):
        if self.__socket.bytesAvailable():
            data = self.__socket.read(8*1024)
            self._parse_bytes(data)
    
    def _handle_events_pending(self):
        while self._events:
            event = self._events.pop(0)
            try:
                if event[0] == 1:
                    self.addedDevice.emit(event[1])
                elif event[0] == 2:
                    self.removedDevice.emit(event[1])
                elif event[0] == 3:
                    self.statusUpdate.emit(event[1], event[2])
            except Exception as e:
                print("error handling pending events: %s" % e)

    def _handle_require_data(self):
        ready = self.__socket.waitForReadyRead(DEFAULT_TIMEOUT * 1000)
        if not ready:
            raise RuntimeError("no response from server")
        data = self.__socket.read(8*1024)
        self._parse_bytes(data)

    def _handle_send_data(self, data):
        bytes_sent = 0
        bytes_total = len(data)
        while bytes_sent < bytes_total:
            n = self.__socket.write(data[bytes_sent:])
            if not n:
                raise RuntimeError("connection closed")
            bytes_sent += n


if __name__ == "__main__":
    from PyQt5 import QtCore
    app = QtCore.QCoreApplication([])

    client = QFaoutClient("localhost")
    devices = client.get_device_list()
    assert devices, "No devices connected"
    print("Connected devices: %s" % (", ".join(devices)))
    print("")

    device = devices[0]
    print("Using: %s" % device)
    print("Status: %s" % client.get_device_status(device))
    
    value = client.read_reg(device, reg=1)
    print("Read register 1: 0x%x" % value)
    
    client.write_reg(device, reg=1, val=value+1)
    print("Write register 1: 0x%x" % (value+1))

    # setup signal handlers for asynchronous events
    def added(dev):
        print("Added: %s" % dev)
    def removed(dev):
        print("Removed: %s" % dev)
    def status(dev, stat):
        print("Status %s: %s" % (dev, stat))
    client.addedDevice.connect(added)
    client.removedDevice.connect(removed)
    client.statusUpdate.connect(status)

    # stop Qt event loop on CTRL+C
    import signal
    signal.signal(signal.SIGINT, lambda *args: app.quit())
    # ensure Qt regulary calls back into the python interpreter 
    timer = QtCore.QTimer()
    timer.start(100)
    timer.timeout.connect(lambda: None)
    
    # run qt event loop
    print("")
    print("Waiting for events")
    app.exec_()