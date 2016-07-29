#-----------------------------------------------------------------------------
# Author: Peter Würtz, TU Kaiserslautern (2016)
#
# Distributed under the terms of the GNU General Public License Version 3.
# The full license is in the file COPYING.txt, distributed with this software.
#-----------------------------------------------------------------------------

import numpy as np

REG_CMD = (0, 0)
REG_STATUS = (0, 1)
REG_CONFIG = (0, 2)
REG_VERSION = (0, 3)
REG_ADCPROG = (3, 0)


class DigitizerMixin(object):

    def write_cmd_bit(self, bit):
        self.write_reg(*REG_CMD, val=(1 << bit))

    def get_version(self):
        return self.read_reg(*REG_VERSION)

    def get_config_bit(self, bit):
        conf_val = self.read_reg(*REG_CONFIG)
        return bool(conf_val & (1 << bit))

    def get_config_bits(self, nbits, offset):
        conf_val = self.read_reg(*REG_CONFIG)
        mask = 2**nbits - 1
        return (conf_val >> offset) & mask

    def set_config_bit(self, bit, enabled):
        conf_val = self.read_reg(*REG_CONFIG)
        if enabled:
            conf_val |= (1 << bit)
        else:
            conf_val &= ~(1 << bit)
        self.write_reg(*REG_CONFIG, val=conf_val)

    def set_config_bits(self, value, nbits, offset):
        conf_val = self.read_reg(*REG_CONFIG)
        mask = 2**nbits - 1
        value &= mask
        # zero selected bit range, then apply value
        conf_val &= ~(mask << offset)
        conf_val |= (value << offset)
        self.write_reg(*REG_CONFIG, val=conf_val)

    def get_status(self):
        return self.__status_to_dict(self.read_reg(*REG_STATUS))

    @staticmethod
    def __status_to_dict(status_val):
        return {
            "adc_clk_locked": bool(status_val & 1<<0),
            "sample_buffer_full": bool(status_val & 1<<1),
            "sample_buffer_empty": bool(status_val & 1<<2),
        }

    ###################################################

    def threshold(self, value=None):
        if value is None:
            return float(self.from_signed_12bit(self.read_reg(0, 4)))
        else:
            value_raw = int(self.to_signed_12bit(value))
            self.write_reg(0, 4, value_raw)

    def sample_invert(self, invert=None):
        if invert is None:
            return self.get_config_bit(4)
        else:
            self.set_config_bit(4, invert)

    def sample_average(self, n=None):
        if n is None:
            return self.get_config_bits(2, 5)
        else:
            self.set_config_bits(n, 2, 5)

    def sample_source(self, channel=None):
        if channel is None:
            return self.get_config_bits(2, 0)
        else:
            self.set_config_bits(channel, 2, 0)

    def sample_buf_trigger(self, src=None):
        if src is None:
            return self.get_config_bits(2, 2)
        else:
            self.set_config_bits(src, 2, 2)

    def sample_buf_reset(self):
        self.write_cmd_bit(1)

    def sample_buf_write(self, samples):
        samples_raw = self.to_signed_12bit(samples)
        self.write_reg_n(2, 1, samples_raw)

    def sample_buf_read(self, raw=False):
        # read maxfinder records
        n = self.read_reg(2, 0)
        samplebuf_raw = self.read_reg_n(2, 1, n).astype("u2")
        if raw:
            return samplebuf_raw
        if not samplebuf_raw.size:
            return 0, np.array([])

        # interpret data
        trig_offset = samplebuf_raw[0] / float(2**8)  # first word is trigger offset (8 fractional bits)
        trig_offset *= 2.  # trig_offset is in units of 1/clk_main, convert to units of 1/clk_sample
        samples = self.from_signed_12bit(samplebuf_raw[1:])  # convert samples to float
        return trig_offset, samples

    def sample_buf_maxfind_read(self, raw=False):
        n = self.read_reg(2, 2)
        maxfind_raw = self.read_reg_n(2, 3, n)
        if raw:
            return maxfind_raw

        # interpret data
        maxfind_raw = maxfind_raw.astype(np.uint32)
        maxfind_raw = (maxfind_raw[0::2] << 16) | maxfind_raw[1::2]  # reassemble 32 bit words
        maxfind_heights = maxfind_raw & (2**12-1)  # extract height bits
        maxfind_heights = self.from_signed_12bit(maxfind_heights)  # convert height to float
        maxfind_pos = (maxfind_raw >> 12).astype(float)  # extract position bits
        maxfind_pos /= float(2**8)  # position contains 8 fractional bits
        maxfind_pos *= 2. # time is in units of 1/clk_main, convert to units of 1/clk_sample
        return maxfind_pos, maxfind_heights

    ###################################################

    def _write_adcprog(self, addr, data):
        word = ((addr & 0x7f) << 8) | (data & 0xff)
        self.write_reg(*REG_ADCPROG, val=word)

    def adcprog_reset(self):
        self._write_adcprog(0, 1 << 7)

    def adcprog_powerdown(self, sleep):
        self._write_adcprog(1, 1 << 3 if sleep else 0)

    def adcprog_timing(self, clkphase=0, dcs=True, clkinv=False):
        word = ((clkinv & 0x1) << 3) | ((clkphase & 0x3) << 1) | (dcs & 0x1)
        self._write_adcprog(2, word)

    def adcprog_output(self, ilvds=0, termon=False, outoff=False):
        word = ((ilvds & 0x7) << 2) | ((termon & 0x1) << 1) | (outoff & 0x1)
        self._write_adcprog(3, word)

    def adcprog_data_test(self, pattern):
        word = ((pattern & 0x7) << 5) | (1 << 2)
        self._write_adcprog(4, word)

    def adcprog_data_normal(self, twoscomp=False, rand=False, abp=False):
        word = ((abp & 0x1) << 4) | ((rand & 0x1) << 1) | (twoscomp & 0x1)
        self._write_adcprog(4, word)

    ###################################################

    @staticmethod
    def from_signed_12bit(data):
        data = np.asarray(data, dtype="u2").copy()
        sign_bit = (data >> 11) & 0x1  # 12th bit is sign bit
        # resize 12bit signed to 16bit signed and cast to correct type
        data &= 0x7ff
        data |= ((0xffff*sign_bit) & 0xf800)
        data = data.view("i2").astype(float)
        data *= (1. / (2**11-1))
        return data

    @staticmethod
    def to_signed_12bit(data):
        data = np.asfarray(data).copy()
        data *= 2**11-1
        data = data.astype(np.int16).view(np.uint16)
        data &= 2**12-1
        return data
