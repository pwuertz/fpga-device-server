
SDRAM_MAX_ADDR = 2**23-1

ADDR_REGS = 0
REG_CMD = 0
REG_STATUS = 1
REG_CONFIG = 2
REG_VERSION = 3

ADDR_SDRAM = 1
ADDR_DAC = 2
ADDR_INTERP = 3
ADDR_SEQ = 4

class FaoutMixin(object):

    def get_device_status(self):
        value = self.read_reg(ADDR_REGS, REG_STATUS)
        return FaoutMixin._status_to_dict(value)

    def get_config_bit(self, bit):
        conf_reg = self.read_reg(ADDR_REGS, REG_CONFIG)
        return bool(conf_reg & (1 << bit))

    def set_config_bit(self, bit, enabled):
        conf_reg = self.read_reg(ADDR_REGS, REG_CONFIG)
        if enabled:
            conf_reg |= (1 << bit)
        else:
            conf_reg &= ~(1 << bit)
        self.write_reg(ADDR_REGS, REG_CONFIG, conf_reg)

    def get_version(self):
        return self.read_reg(ADDR_REGS, REG_VERSION)

    def reset(self):
        self.write_reg(ADDR_REGS, REG_CMD, 0x1 << 0)

    def sequence_start(self):
        self.write_reg(ADDR_REGS, REG_CMD, 0x1 << 1)

    def sequence_stop(self):
        self.write_reg(ADDR_REGS, REG_CMD, 0x1 << 2)

    def sequence_hold(self):
        self.write_reg(ADDR_REGS, REG_CMD, 0x1 << 3)

    def sequence_arm(self):
        self.write_reg(ADDR_REGS, REG_CMD, 0x1 << 4)


    def sdram_rewind(self):
        self.write_reg(ADDR_REGS, REG_CMD, 0x1 << 5)

    def sdram_clear(self):
        self.write_reg(ADDR_REGS, REG_CMD, 0x1 << 6)

    def sdram_rd_wr_ptr(self):
        rd_lw = self.read_reg(ADDR_SDRAM, 0)
        rd_hw = self.read_reg(ADDR_SDRAM, 1)
        rd_ptr = (rd_hw << 16) | rd_lw
        wr_lw = self.read_reg(ADDR_SDRAM, 2)
        wr_hw = self.read_reg(ADDR_SDRAM, 3)
        wr_ptr = (wr_hw << 16) | wr_lw
        return rd_ptr, wr_ptr

    def sdram_write(self, data):
        rd_ptr, wr_ptr = self.sdram_rd_wr_ptr()
        n_free = SDRAM_MAX_ADDR - wr_ptr
        if len(data) > n_free:
            raise ValueError("too much data for sdram")
        self.write_reg_n(ADDR_SDRAM, 4, data)

    def sdram_read(self, n_words=None):
        rd_ptr, wr_ptr = self.sdram_rd_wr_ptr()
        n_available = wr_ptr - rd_ptr
        if n_words is None:
            n_words = n_available
        if n_words > n_available:
            raise ValueError("not enough data in sdram")
        return self.read_reg_n(ADDR_SDRAM, 4, n_words)

    def get_clock_extern(self):
        return self.get_config_bit(0)

    def set_clock_extern(self, enabled):
        return self.set_config_bit(0, enabled)


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

    def write_dac(self, dac_index, value):
        if dac_index < 0 or dac_index >= 6:
            raise ValueError("dac_index out of bounds")
        self.write_reg(ADDR_DAC, dac_index, value)

    def read_dac(self, dac_index):
        if dac_index < 0 or dac_index >= 6:
            raise ValueError("dac_index out of bounds")
        return self.read_reg(ADDR_DAC, dac_index)

    def write_interp(self, index, value, steps):
        if index < 0 or index >= 6:
            raise ValueError("dac_index out of bounds")
        self.write_reg(ADDR_INTERP, index+6, steps)
        self.write_reg(ADDR_INTERP, index, value)

    def read_interp(self, index):
        if index < 0 or index >= 6:
            raise ValueError("dac_index out of bounds")
        steps = self.read_reg(ADDR_INTERP, index+6)
        value = self.read_reg(ADDR_INTERP, index)
        return value, steps


