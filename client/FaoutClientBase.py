# -*- coding: utf-8 -*-
import msgpack
import numpy as np
import socket

ADDR_REGS = 0
REG_CMD = 0
REG_STATUS = 1
REG_CONFIG = 2
REG_VERSION = 3

ADDR_SDRAM = 1
ADDR_DAC = 2
ADDR_INTERP = 3
ADDR_SEQ = 4

SDRAM_MAX_ADDR = 2**23-1

class FaoutClientBase(object):
    RPC_RCODE_ERROR = -1
    RPC_RCODE_OK = 0
    RPC_RCODE_ADDED = 1
    RPC_RCODE_REMOVED = 2
    RPC_RCODE_REG_CHANGED = 3    
    
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
            packet = list(packet)
            try:
                rcode = int(packet[0])
            except:
                raise ValueError("invalid packet received")

            if rcode <= 0:
                self._answers.append(packet)
            else:
                self._events.append(packet)

        if self._events and self.__events_pending_cb:
            self.__events_pending_cb()

    def _wait_for_answer(self):
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
        return self._wait_for_answer()[1]

    def read_reg(self, serial, addr, port):
        self._send_object(["readreg", serial, addr, port])
        return self._wait_for_answer()[1]

    def write_reg(self, serial, addr, port, val):
        self._send_object(["writereg", serial, addr, port, val])
        self._wait_for_answer()
        return

    def write_reg_n(self, serial, addr, port, data):
        data_raw_be = bytes(np.asarray(data, dtype=">u2").data)
        self._send_object(["writeregn", serial, addr, port, data_raw_be])
        self._wait_for_answer()
        return

    def read_reg_n(self, serial, addr, port, n_words):
        self._send_object(["readregn", serial, addr, port, n_words])
        data_raw_be = self._wait_for_answer()[1]
        return np.frombuffer(data_raw_be, dtype=">u2", count=n_words)

    # TODO: should these functions be common for all FAOUT devices?

    def get_device_status(self, serial):
        value = self.read_reg(serial, ADDR_REGS, REG_STATUS)
        return FaoutClientBase._status_to_dict(value)

    def reset(self, serial):
        self.write_reg(serial, ADDR_REGS, REG_CMD, 0x1 << 0)

    def sequence_start(self, serial):
        self.write_reg(serial, ADDR_REGS, REG_CMD, 0x1 << 1)

    def sequence_stop(self, serial):
        self.write_reg(serial, ADDR_REGS, REG_CMD, 0x1 << 2)

    def sequence_hold(self, serial):
        self.write_reg(serial, ADDR_REGS, REG_CMD, 0x1 << 3)

    def sequence_arm(self, serial):
        self.write_reg(serial, ADDR_REGS, REG_CMD, 0x1 << 4)


    def sdram_rewind(self, serial):
        self.write_reg(serial, ADDR_REGS, REG_CMD, 0x1 << 5)

    def sdram_clear(self, serial):
        self.write_reg(serial, ADDR_REGS, REG_CMD, 0x1 << 6)

    def sdram_rd_wr_ptr(self, serial):
        rd_lw = self.read_reg(serial, ADDR_SDRAM, 0)
        rd_hw = self.read_reg(serial, ADDR_SDRAM, 1)
        rd_ptr = (rd_hw << 16) | rd_lw
        wr_lw = self.read_reg(serial, ADDR_SDRAM, 2)
        wr_hw = self.read_reg(serial, ADDR_SDRAM, 3)
        wr_ptr = (wr_hw << 16) | wr_lw
        return rd_ptr, wr_ptr

    def sdram_write(self, serial, data):
        rd_ptr, wr_ptr = self.sdram_rd_wr_ptr(serial)
        n_free = SDRAM_MAX_ADDR - wr_ptr
        if len(data) > n_free:
            raise ValueError("too much data for sdram")
        self.write_reg_n(serial, ADDR_SDRAM, 4, data)

    def sdram_read(self, serial, n_words=None):
        rd_ptr, wr_ptr = self.sdram_rd_wr_ptr(serial)
        n_available = wr_ptr - rd_ptr
        if n_words is None:
            n_words = n_available
        if n_words > n_available:
            raise ValueError("not enough data in sdram")
        return self.read_reg_n(serial, ADDR_SDRAM, 4, n_words)


    def get_version(self, serial):
        return self.read_reg(serial, ADDR_REGS, REG_VERSION)

    def get_config_bit(self, serial, bit):
        conf_reg = self.read_reg(serial, ADDR_REGS, REG_CONFIG)
        return bool(conf_reg & (1 << bit))

    def set_config_bit(self, serial, bit, enabled):
        conf_reg = self.read_reg(serial, ADDR_REGS, REG_CONFIG)
        if enabled:
            conf_reg |= (1 << bit)
        else:
            conf_reg &= ~(1 << bit)
        self.write_reg(serial, ADDR_REGS, REG_CONFIG, conf_reg)

    def get_clock_extern(self, serial):
        return self.get_config_bit(serial, 0)

    def set_clock_extern(self, serial, enabled):
        return self.set_config_bit(serial, 0, enabled)


    # TODO: the following functions are device dependent

    @staticmethod
    def _status_to_dict(status_val):
        return {
            "status": status_val & 0x7,
            "running": bool(status_val & 1<<3),
            "fifo_full": bool(status_val & 1<<4),
            "fifo_empty": bool(status_val & 1<<5),
            "sdram_empty": bool(status_val & 1<<6),
            "sdram_full": bool(status_val & 1<<7),
            "seq_error": bool(status_val & 1<<8),
            "comm_error": bool(status_val & 1<<9),
            "clk_ext_valid": bool(status_val & 1<<10),
            "clk_ext_selected": bool(status_val & 1<<11),
        }

    def write_dac(self, serial, dac_index, value):
        if dac_index < 0 or dac_index >= 6:
            raise ValueError("dac_index out of bounds")
        self.write_reg(serial, ADDR_DAC, dac_index, value)

    def read_dac(self, serial, dac_index):
        if dac_index < 0 or dac_index >= 6:
            raise ValueError("dac_index out of bounds")
        return self.read_reg(serial, ADDR_DAC, dac_index)

    def write_interp(self, serial, index, value, steps):
        if index < 0 or index >= 6:
            raise ValueError("dac_index out of bounds")
        self.write_reg(serial, ADDR_INTERP, index+6, steps)
        self.write_reg(serial, ADDR_INTERP, index, value)

    def read_interp(self, serial, index):
        if index < 0 or index >= 6:
            raise ValueError("dac_index out of bounds")
        steps = self.read_reg(serial, ADDR_INTERP, index+6)
        value = self.read_reg(serial, ADDR_INTERP, index)
        return value, steps


class SimpleFaoutClient(FaoutClientBase):
    DEFAULT_TIMEOUT = 5

    def __init__(self, host, port=9001):
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
    import argparse
    parser = argparse.ArgumentParser(description='Faout-Client Example')
    parser.add_argument('host', nargs='?', default='localhost')
    parser.add_argument('port', nargs='?', type=int, default=9002)
    args = parser.parse_args()

    client = SimpleFaoutClient(args.host, args.port)
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

    print("")
    print("Waiting for events")
    while 1:
        for event in client.wait_for_events():
            print(event)

