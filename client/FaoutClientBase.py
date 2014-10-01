# -*- coding: utf-8 -*-
from docutils.nodes import classifier
import msgpack
import socket

class FaoutClientBase(object):
    def __init__(self, send_data_cb, require_data_cb, events_pending_cb=None):
        self.__send_data_cb = send_data_cb
        self.__require_data_cb = require_data_cb
        self.__events_pending_cb = events_pending_cb
        self.__unpacker = msgpack.Unpacker()
        self._events = []
        self._answers = []

    def _send_object(self, obj):
        data = msgpack.packb(obj)
        self.__send_data_cb(data)

    def _parse_bytes(self, data):
        self.__unpacker.feed(data)
        for packet in self.__unpacker:
            try:
                rcode = int(packet[0])
            except:
                raise ValueError("invalid packet received")

            if rcode <= 0:
                self._answers.append(packet)
            else:
                if rcode == 3:
                    try:
                        packet[2] = FaoutClientBase.__status_to_dict(packet[2])
                    except:
                        raise ValueError("error processing status event packet")
                self._events.append(packet)

        if self._events and self.__events_pending_cb:
            self.__events_pending_cb()

    def wait_for_answer(self):
        if not self._answers and not self.__require_data_cb:
            raise RuntimeError("cannot wait for data without require_data_cb")
        while not self._answers:
            self.__require_data_cb()
        packet = self._answers.pop(0)
        if packet[0] != 0:
            raise RuntimeError("returned error: %s" % packet)
        else:
            return packet

    def get_device_list(self):
        self._send_object(["devicelist"])
        return self.wait_for_answer()[1]

    def get_device_status(self, serial):
        self._send_object(["status", serial])
        value = self.wait_for_answer()[1]
        return FaoutClientBase.__status_to_dict(value)

    def read_reg(self, serial, reg):
        self._send_object(["readreg", serial, reg])
        return self.wait_for_answer()[1]

    def write_reg(self, serial, reg, val):
        self._send_object(["writereg", serial, reg, val])
        self.wait_for_answer()
        return

    def write_ram(self, serial, data):
        self._send_object(["writeram", serial, data])
        self.wait_for_answer()
        return

    def sequence_reset(self, serial):
        self.write_reg(serial, 0, 0x1 << 0)

    def sequence_start(self, serial):
        self.write_reg(serial, 0, 0x1 << 1)

    def sequence_stop(self, serial):
        self.write_reg(serial, 0, 0x1 << 2)

    @staticmethod
    def __status_to_dict(status_val):
        return {
            "status": status_val & 0xf,
            "prepared": bool(status_val & 1<<4),
            "running": bool(status_val & 1<<5),
            "sdram_empty": bool(status_val & 1<<6),
            "sdram_full": bool(status_val & 1<<7),
            "seq_error": bool(status_val & 1<<8),
            "comm_error": bool(status_val & 1<<9),
        }

    def get_ram_read_ptr(self, serial):
        rd_lw = self.read_reg(serial, 6)
        rd_hw = self.read_reg(serial, 7)
        return (rd_hw << 16) | rd_lw

    def get_ram_write_ptr(self, serial):
        wr_lw = self.read_reg(serial, 8)
        wr_hw = self.read_reg(serial, 9)
        return (wr_hw << 16) | wr_lw

    def write_dac1(self, serial, value):
        self.write_reg(serial, 3, value)

    def write_dac2(self, serial, value):
        self.write_reg(serial, 4, value)

    def read_dac1(self, serial):
        return self.read_reg(serial, 3)

    def read_dac2(self, serial):
        return self.read_reg(serial, 4)

    def write_display(self, serial, value):
        self.write_reg(serial, 1, value)

    def read_display(self, serial):
        return self.read_reg(serial, 1)


class SimpleFaoutClient(FaoutClientBase):
    DEFAULT_TIMEOUT = 5

    def __init__(self, host, port=9000):
        FaoutClientBase.__init__(self,
                                 send_data_cb=self._handle_send_data,
                                 require_data_cb=self._handle_require_data)
        self.__socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.__socket.settimeout(SimpleFaoutClient.DEFAULT_TIMEOUT)
        self.__socket.connect((host, port))

    def _handle_require_data(self):
        data = self.__socket.recv(8*1024)
        if not data:
            raise RuntimeError("no response from server")
        self._parse_bytes(data)

    def _handle_send_data(self, data):
        bytes_sent = 0
        bytes_total = len(data)
        while bytes_sent < bytes_total:
            n = self.__socket.send(data[bytes_sent:])
            if not n:
                raise RuntimeError("connection closed")
            bytes_sent += n

    def wait_for_events(self):
        while not self._events:
            try:
                data = self.__socket.recv(8*1024)
                self._parse_bytes(data)
            except socket.timeout:
                pass
        events_copy = list(self._events)
        del self._events[:]
        return events_copy


if __name__ == "__main__":
    client = SimpleFaoutClient("localhost")
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

    print("")
    print("Waiting for events")
    while 1:
        for event in client.wait_for_events():
            print(event)
