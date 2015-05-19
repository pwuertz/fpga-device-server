from FaoutClientBase import FaoutClientBase, ADDR_REGS, REG_STATUS
from PyQt5 import QtCore, QtNetwork


class QFaoutClient(QtCore.QObject, FaoutClientBase):
    DEFAULT_TIMEOUT = 5

    deviceAdded = QtCore.pyqtSignal(str)
    deviceRemoved = QtCore.pyqtSignal(str)
    statusChanged = QtCore.pyqtSignal(str, object)
    registerChanged = QtCore.pyqtSignal(str, int, int, int)
    __eventsPending = QtCore.pyqtSignal()

    def __init__(self, host, port=9001):
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
        self.__socket.waitForConnected(QFaoutClient.DEFAULT_TIMEOUT * 1000)

    def _handle_ready_read(self):
        if self.__socket.bytesAvailable():
            data = self.__socket.read(self.__socket.bytesAvailable())
            self._parse_bytes(data)
    
    def _handle_events_pending(self):
        while self._events:
            event = self._events.pop(0)
            try:
                if event[0] == FaoutClientBase.RPC_RCODE_ADDED:
                    self.deviceAdded.emit(event[1])
                elif event[0] == FaoutClientBase.RPC_RCODE_REMOVED:
                    self.deviceRemoved.emit(event[1])
                elif event[0] == FaoutClientBase.RPC_RCODE_REG_CHANGED:
                    serial, addr, port, value = event[1:5]
                    if addr == ADDR_REGS and port == REG_STATUS:
                        status = self._status_to_dict(value)
                        self.statusChanged.emit(serial, status)
                    else:
                        self.registerChanged.emit(serial, addr, port, value)
            except Exception as e:
                print("error handling pending events: %s" % e)

    def _handle_require_data(self):
        ready = self.__socket.waitForReadyRead(QFaoutClient.DEFAULT_TIMEOUT * 1000)
        if not ready:
            raise RuntimeError("no response from server")
        data = self.__socket.read(self.__socket.bytesAvailable())
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
    import argparse
    parser = argparse.ArgumentParser(description='Faout-Client Example')
    parser.add_argument('host', nargs='?', default='localhost')
    parser.add_argument('port', nargs='?', type=int, default=9002)
    args = parser.parse_args()

    from PyQt5 import QtCore
    app = QtCore.QCoreApplication([])

    client = QFaoutClient(args.host, args.port)
    print 'Connected to %s' % args.host
    devices = client.get_device_list()
    assert devices, "No devices connected"
    print("Connected devices: %s" % (", ".join(devices)))
    print("")

    device = devices[0]
    print("Using: %s" % device)
    print("Status: %s" % client.get_device_status(device))

    value = client.get_version(device)
    print("Device version: %d" % value)

    # setup signal handlers for asynchronous events
    def added(dev):
        print("Added: %s" % dev)
    def removed(dev):
        print("Removed: %s" % dev)
    def status(dev, stat):
        print("Status %s: %s" % (dev, stat))
    def regchanged(dev, addr, port, value):
        print("Register (%s, %d, %d): %d" % (dev, addr, port, value))
    client.deviceAdded.connect(added)
    client.deviceRemoved.connect(removed)
    client.statusChanged.connect(status)
    client.registerChanged.connect(regchanged)

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

