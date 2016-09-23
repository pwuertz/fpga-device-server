# -*- coding: utf-8 -*-
#-----------------------------------------------------------------------------
# Author: Peter Würtz, TU Kaiserslautern (2016)
#
# Distributed under the terms of the GNU General Public License Version 3.
# The full license is in the file COPYING.txt, distributed with this software.
#-----------------------------------------------------------------------------

from pyfpgaclient.FpgaClientBase import FpgaClientBase, FpgaDevice
from PyQt5 import QtCore, QtNetwork


class QFpgaDevice(QtCore.QObject, FpgaDevice):

    registerChanged = QtCore.pyqtSignal(int, int, int)
    __registerChangedQueue = QtCore.pyqtSignal(int, int, int)

    def __init__(self, client, serial):
        QtCore.QObject.__init__(self, client=client, serial=serial)
        self.__registerChangedQueue.connect(self.registerChanged, type=QtCore.Qt.QueuedConnection)

    def _reg_changed(self, addr, port, value):
        self.__registerChangedQueue.emit(addr, port, value)


class QFpgaClient(QtCore.QObject, FpgaClientBase):
    DEFAULT_TIMEOUT = 5
    DEVICE_BASE_CLASS = QFpgaDevice

    deviceAdded = QtCore.pyqtSignal(str, object)
    deviceRemoved = QtCore.pyqtSignal(str)
    __deviceAddedQueue = QtCore.pyqtSignal(str, object)
    __deviceRemovedQueue = QtCore.pyqtSignal(str)

    def __init__(self, host, port=9002):
        QtCore.QObject.__init__(self)
        # always queue added, removed or event callbacks in order to
        # prevent event processing within synchronous calls
        self.__deviceAddedQueue.connect(self.deviceAdded, type=QtCore.Qt.QueuedConnection)
        self.__deviceRemovedQueue.connect(self.deviceRemoved, type=QtCore.Qt.QueuedConnection)

        # setup tcp socket
        self.__host = host
        self.__port = port
        self.__socket = QtNetwork.QTcpSocket()
        self.__socket.readyRead.connect(self.__handle_ready_read)
        # TODO: handle other signals, like disconnected or error

    def connect(self):
        if not self.__socket.isOpen():
            # connect to server
            self.__socket.connectToHost(self.__host, self.__port)
            if (not self.__socket.waitForConnected(QFpgaClient.DEFAULT_TIMEOUT * 1000)):
                raise RuntimeError("Could not connect: %s" % self.__socket.errorString())
            # initialize device map by getting the device list
            self.get_device_list()

    def disconnect(self):
        # remove all devices
        device_list = list(self._devices.keys())
        for serial in device_list:
            self.__handle_removed(serial)

        # disconnect
        if self.__socket.isOpen():
            self.__socket.close()

    def _device_added(self, serial, device):
        self.__deviceAddedQueue.emit(serial, device)

    def _device_removed(self, serial):
        self.__deviceRemovedQueue.emit(serial)

    def __handle_ready_read(self):
        if self.__socket.bytesAvailable():
            data = self.__socket.read(self.__socket.bytesAvailable())
            self._parse_data(data)

    def _write_data(self, data):
        bytes_sent = 0
        bytes_total = len(data)
        while bytes_sent < bytes_total:
            n = self.__socket.write(data[bytes_sent:])
            if n <= 0:
                raise RuntimeError("could not write data")
            bytes_sent += n

    def _require_data(self):
        ready = self.__socket.waitForReadyRead(QFpgaClient.DEFAULT_TIMEOUT * 1000)
        if not ready:
            raise RuntimeError("no response from server")
        data = self.__socket.read(self.__socket.bytesAvailable())
        self._parse_data(data)
