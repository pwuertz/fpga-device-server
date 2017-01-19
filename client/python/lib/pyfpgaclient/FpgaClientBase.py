# -*- coding: utf-8 -*-
#-----------------------------------------------------------------------------
# Author: Peter Würtz, TU Kaiserslautern (2016)
#
# Distributed under the terms of the GNU General Public License Version 3.
# The full license is in the file COPYING.txt, distributed with this software.
#-----------------------------------------------------------------------------

import msgpack
import numpy as np
import socket
import warnings
from six import text_type, PY3

_DEFAULT_DEVICE_MIXIN_MAP = {}


class FpgaDevice(object):
    def __init__(self, client, serial):
        self._client = client
        self._serial = serial

    def _reg_changed(self, addr, port, value):
        # implement method for handling register changes
        pass

    def __str__(self):
        return self._serial

    def __repr__(self):
        return "<%s: %s>" % (type(self).__name__, self._serial)

    def reprogram_device(self):
        return self._client.reprogram_device(self._serial)

    def read_reg(self, addr, port):
        return self._client.read_reg(self._serial, addr, port)

    def write_reg(self, addr, port, val):
        return self._client.write_reg(self._serial, addr, port, val)

    def write_reg_n(self, addr, port, data):
        return self._client.write_reg_n(self._serial, addr, port, data)

    def read_reg_n(self, addr, port, n_words):
        return self._client.read_reg_n(self._serial, addr, port, n_words)

    def _write_raw(self, data):
        return self._client.write_raw(self._serial, data)

    def _read_raw(self, n_bytes):
        return self._client.read_raw(self._serial, n_bytes)


class FpgaClientBase(object):
    RPC_RCODE_ERROR = -1
    RPC_RCODE_OK = 0
    RPC_RCODE_ADDED = 1
    RPC_RCODE_REMOVED = 2
    RPC_RCODE_REG_CHANGED = 3

    DEVICE_MIXIN_MAP = _DEFAULT_DEVICE_MIXIN_MAP
    DEVICE_BASE_CLASS = FpgaDevice

    def __init__(self):
        self.__unpacker = msgpack.Unpacker()
        self._answers = []
        self._devices = {}

    @classmethod
    def registerDeviceMixin(cls, serial_prefix, DeviceMixin):
        """
        When creating a new device instance from DEVICE_BASE_CLASS,
        also inherit from the DeviceMixin class if the serial_prefix of
        the new device matches.
        
        :serial_prefix: prefix for device identification
        :DeviceMixin: mixin class providing additional methods for a specific devices
        """
        cls.DEVICE_MIXIN_MAP[serial_prefix] = DeviceMixin

    def _write_data(self):
        # implement method for handling data to be written
        raise NotImplementedError()

    def _require_data(self):
        # implement method for handling requests for more data
        raise NotImplementedError()

    def _device_added(self, serial, device):
        # implement method for handling added devices
        pass

    def _device_removed(self, serial):
        # implement method for handling removed devices
        pass

    def _parse_data(self, data):
        # use method for handling incoming data
        self.__unpacker.feed(data)
        for packet in self.__unpacker:
            packet = list(packet)
            try:
                rcode = int(packet[0])
            except:
                raise ValueError("invalid packet received")

            if rcode <= 0:
                self._answers.append(packet)
            elif rcode == FpgaClientBase.RPC_RCODE_ADDED:
                serial = packet[1].decode() if PY3 else packet[1]
                self.__handle_added(serial=serial)
            elif rcode == FpgaClientBase.RPC_RCODE_REMOVED:
                serial = packet[1].decode() if PY3 else packet[1]
                self.__handle_removed(serial=serial)
            elif rcode == FpgaClientBase.RPC_RCODE_REG_CHANGED:
                serial, addr, port, value = packet[1:5]
                serial = serial.decode() if PY3 else serial
                self.__handle_reg_changed(serial, addr, port, value)
            else:
                warnings.warn("unknown packet type (rcode=%d)" % rcode)

    def _wait_for_answer(self):
        while not self._answers:
            self._require_data()
        packet = self._answers.pop(0)
        if packet[0] != 0:
            raise RuntimeError("returned error: %s" % packet)
        else:
            return packet

    def __send_object(self, obj):
        data = msgpack.packb(obj, use_bin_type=True)
        self._write_data(data)

    def __handle_added(self, serial):
        if serial in self._devices:
            return

        # find mixin class from map for creating device instance
        for prefix, DeviceMixin in self.DEVICE_MIXIN_MAP.items():
            if serial.startswith(prefix):
                DeviceClass = type('%sDevice' % prefix, (self.DEVICE_BASE_CLASS, DeviceMixin), {})
                self._devices[serial] = DeviceClass(client=self, serial=serial)
                break
        else:
            # no mixin found, create plain device
            self._devices[serial] = self.DEVICE_BASE_CLASS(client=self, serial=serial)

        self._device_added(serial, self._devices[serial])

    def __handle_removed(self, serial):
        if serial not in self._devices:
            return

        del self._devices[serial]
        self._device_removed(serial)

    def __handle_reg_changed(self, serial, addr, port, value):
        # get device object for serial
        try:
            device = self._devices[serial]
        except:
            return  # silently ignore events for unknown devices, TODO: warn/log/report?
        device._reg_changed(addr, port, value)

    def __handle_device_list_changes(self, device_list):
        device_list = [serial.decode() if PY3 else serial for serial in device_list]

        # check for devices that were not added yet
        for serial in device_list:
            if serial not in self._devices:
                self.__handle_added(serial)
        # check for devices that haven't been removed yet
        for serial in list(self._devices.keys()):
            if serial not in device_list:
                self.__handle_removed(serial)

    def get_device_list(self):
        # refresh device list and get internal device list in sync
        self.__send_object(["devicelist"])
        device_list = self._wait_for_answer()[1]
        self.__handle_device_list_changes(device_list)
        return device_list

    def get_device(self, serial):
        return self._devices[serial]

    def reprogram_device(self, serial):
        self.__send_object(["reprogram", serial])
        return self._wait_for_answer()[1]

    def read_reg(self, serial, addr, port):
        self.__send_object(["readreg", serial, addr, port])
        return self._wait_for_answer()[1]

    def write_reg(self, serial, addr, port, val):
        self.__send_object(["writereg", serial, addr, port, val])
        self._wait_for_answer()
        return

    def write_reg_n(self, serial, addr, port, data):
        data_raw_be = bytes(np.asarray(data, dtype=">u2").data)
        self.__send_object(["writeregn", serial, addr, port, data_raw_be])
        self._wait_for_answer()
        return

    def read_reg_n(self, serial, addr, port, n_words):
        self.__send_object(["readregn", serial, addr, port, n_words])
        data_raw_be = self._wait_for_answer()[1]
        return np.frombuffer(data_raw_be, dtype=">u2", count=n_words)

    def write_raw(self, serial, data):
        data_raw = bytes(np.asarray(data, dtype=np.uint8).data)
        self.__send_object(["writeraw", serial, data_raw])
        self._wait_for_answer()
        return

    def read_raw(self, serial, n_bytes):
        self.__send_object(["readraw", serial, n_bytes])
        data_raw = self._wait_for_answer()[1]
        return np.frombuffer(data_raw, dtype=np.uint8, count=n_bytes)


if __name__ == "__main__":
    import socket
    import argparse

    class SimpleFpgaClient(FpgaClientBase):
        DEFAULT_TIMEOUT = 5

        def __init__(self, host, port=9002):
            FpgaClientBase.__init__(self)
            self.__socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.__socket.settimeout(SimpleFpgaClient.DEFAULT_TIMEOUT)
            self.__socket.connect((host, port))
            # initialize device map by getting the device list after connect
            self.get_device_list()

        def _write_data(self, data):
            bytes_sent = 0
            bytes_total = len(data)
            while bytes_sent < bytes_total:
                n = self.__socket.send(data[bytes_sent:])
                if not n:
                    raise RuntimeError("connection closed")
                bytes_sent += n

        def _require_data(self):
            data = self.__socket.recv(8*1024)
            if not data:
                raise RuntimeError("no response from server")
            self._parse_data(data)

        def _device_added(self, serial, device):
            print("Device added: %s" % serial)

        def _device_removed(self, serial):
            print("Device removed: %s" % serial)

        def wait_for_events(self):
            try:
                data = self.__socket.recv(8*1024)
                self._parse_data(data)
            except socket.timeout:
                pass

    parser = argparse.ArgumentParser(description='Fpga-Client Example')
    parser.add_argument('host', nargs='?', default='localhost')
    parser.add_argument('port', nargs='?', type=int, default=9002)
    args = parser.parse_args()

    print("Connecting to %s" % args.host)
    client = SimpleFpgaClient(args.host, args.port)
    print("")
    print("Waiting for events")
    while 1:
        client.wait_for_events()
