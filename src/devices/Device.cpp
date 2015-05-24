#include <chrono>
#include <thread>

#include "libftdi/ftdi.h"
#include "Device.h"

#define CMD_READREG 1
#define CMD_WRITEREG 2
#define CMD_READREG_N 3
#define CMD_WRITEREG_N 4


Device::Device(libusb_device* dev, std::string name) :
		m_name(std::move(name)),
		m_ftdi(nullptr),
		m_tracked_regs()
{
	// open FTDI interface A
	ftdi_context* ftdi_a;
	if ((ftdi_a = ftdi_new()) == nullptr) {
		throw std::runtime_error("ftdi_new failed");
	};
	ftdi_set_interface(ftdi_a, INTERFACE_A);
	if (ftdi_usb_open_dev(ftdi_a, dev) != 0) {
		const char* errorstr = ftdi_get_error_string(ftdi_a);
		ftdi_free(ftdi_a);
		throw std::runtime_error(errorstr);
	}

	// open FTDI interface B
	ftdi_context* ftdi_b;
	if ((ftdi_b = ftdi_new()) == nullptr) {
		throw std::runtime_error("ftdi_new failed");
	};
	ftdi_set_interface(ftdi_b, INTERFACE_B);
	if (ftdi_usb_open_dev(ftdi_b, dev) != 0) {
		const char* errorstr = ftdi_get_error_string(ftdi_b);
		ftdi_free(ftdi_b);
		throw std::runtime_error(errorstr);
	}
	// purge buffers of interface B
	if (ftdi_usb_purge_buffers(ftdi_b) != 0) {
		const char* errorstr = ftdi_get_error_string(ftdi_b);
		ftdi_usb_close(ftdi_b);
		ftdi_free(ftdi_b);
		throw std::runtime_error(errorstr);
	}
	// close interface B
	ftdi_usb_close(ftdi_b);
	ftdi_free(ftdi_b);

	// reset device
	if (ftdi_usb_reset(ftdi_a) != 0) {
		const char* errorstr = ftdi_get_error_string(ftdi_a);
		ftdi_usb_close(ftdi_a);
		ftdi_free(ftdi_a);
		throw std::runtime_error(errorstr);
	}
	std::this_thread::sleep_for(std::chrono::milliseconds(100));

	// set synchronous FIFO mode
	if (ftdi_set_bitmode(ftdi_a, 0, BITMODE_SYNCFF) != 0) {
		const char* errorstr = ftdi_get_error_string(ftdi_a);
		ftdi_usb_close(ftdi_a);
		ftdi_free(ftdi_a);
		throw std::runtime_error(errorstr);
	}
	if (ftdi_set_latency_timer(ftdi_a, 1) != 0) {
		const char* errorstr = ftdi_get_error_string(ftdi_a);
		ftdi_usb_close(ftdi_a);
		ftdi_free(ftdi_a);
		throw std::runtime_error(errorstr);
	};

	m_ftdi = ftdi_a;
}

Device::~Device() {
	if (m_ftdi) {
		ftdi_set_bitmode(m_ftdi, 0xfb, BITMODE_RESET);
		ftdi_usb_close(m_ftdi);
		ftdi_free(m_ftdi);
	}
}

void Device::writeReg(uint8_t addr, uint8_t port, uint16_t value) {
	// send register write command
	uint16_t wr_cmd[] = {
			htobe16((CMD_WRITEREG << 12) | ((addr & 0x3f) << 6) | (port & 0x3f)),
			htobe16(value)
	};
	int n = ftdi_write_data(m_ftdi, (unsigned char*) wr_cmd, sizeof(wr_cmd));
	if (n != sizeof(wr_cmd)) {
		throw std::runtime_error("FTDI write error");
	}
}

void ftdi_read_data_wait(struct ftdi_context *ftdi, unsigned char *buf, int size) {
	// TODO: proper timeout implementation

	// wait for the response
	// loop for a maximum duration of 1s
	int sz_recv = 0;
	for (int i = 0; i < 100; ++i) {
		// try to read the remaining bytes
		int n = ftdi_read_data(ftdi, buf + sz_recv, size - sz_recv);
		if (n < 0) {
			std::cerr << "FTDI read error " << n << std::endl;
			throw std::runtime_error("FTDI read error");
		}
		sz_recv += n;
		// continue reading if there are bytes left
		if (sz_recv != size) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} else {
			break;
		}
	}
	if (sz_recv != size) {
		std::cerr << "FTDI read timeout" << std::endl;
		throw std::runtime_error("FTDI read timeout");
	}
}

void Device::readReg(uint8_t addr, uint8_t port, uint16_t* value) {
	// send register read command
	uint16_t rd_cmd[] = {
			htobe16((CMD_READREG << 12) | ((addr & 0x3f) << 6) | (port & 0x3f))
	};
	int n = ftdi_write_data(m_ftdi, (unsigned char*) rd_cmd, sizeof(rd_cmd));
	if (n != sizeof(rd_cmd)) {
		throw std::runtime_error("FTDI write error");
	}

	// read result
	uint16_t value_be;
	ftdi_read_data_wait(m_ftdi, (unsigned char*) &value_be, sizeof(uint16_t));
	*value = be16toh(value_be);

	// store result if tracked and invoke callback
	auto it = m_tracked_regs.find(addr_port_t(addr, port));
	if (it != m_tracked_regs.end()) {
		uint16_t value_old = it->second;
		it->second = *value;
		if (*value != value_old && m_device_reg_change_cb) m_device_reg_change_cb(m_name, addr, port, *value);
	}
}

void Device::writeRegN(uint8_t addr, uint8_t port, const uint16_t* data_be, size_t n) {
	// send N words to register

	// packet length encoded as 16bit unsigned, send data in chunks of n_packet_max
	const size_t n_packet_max = (1<<16)-1;
	uint16_t out_buffer[2 + n_packet_max];
	out_buffer[0] = htobe16((CMD_WRITEREG_N << 12) | ((addr & 0x3f) << 6) | (port & 0x3f));

	size_t n_sent = 0;
	while (n_sent != n) {
		// determine size of next packet
		size_t n_packet = std::min(n-n_sent, n_packet_max);
		out_buffer[1] = htobe16(n_packet);
		// copy data to output buffer
		for (size_t i = 0; i < n_packet; ++i) {
			out_buffer[2+i] = data_be[n_sent+i];
		}

		size_t packet_bytes = sizeof(uint16_t) * (2 + n_packet);
		int n = ftdi_write_data(m_ftdi, (unsigned char*) out_buffer, packet_bytes);
		if (n <= 0) {
			std::cerr << "FTDI write error " << n << std::endl;
			throw std::runtime_error("FTDI write error");
		} else if (n < int(packet_bytes)) {
			throw std::runtime_error("FTDI unhandled partial write");
		}

		n_sent += n_packet;
	}
}

void Device::readRegN(uint8_t addr, uint8_t port, uint16_t* data_be, size_t n) {
	// read N words from register

	// packet length encoded as 16bit unsigned, read data in chunks of n_packet_max
	const size_t n_packet_max = (1<<16)-1;
	uint16_t rdn_cmd[] = {
			htobe16((CMD_READREG_N << 12) | ((addr & 0x3f) << 6) | (port & 0x3f)),
			0
	};

	size_t n_read = 0;
	while (n_read != n) {
		// determine size of next packet
		size_t n_packet = std::min(n-n_read, n_packet_max);
		rdn_cmd[1] = htobe16(n_packet);
		// send read request
		int n = ftdi_write_data(m_ftdi, (unsigned char*) rdn_cmd, sizeof(rdn_cmd));
		if (n != sizeof(rdn_cmd)) {
			throw std::runtime_error("FTDI write error");
		}

		// read data
		size_t sz_packet = sizeof(uint16_t) * n_packet;
		ftdi_read_data_wait(m_ftdi, (unsigned char*) (data_be + n_read), sz_packet);
		n_read += n_packet;
	}
}

void Device::trackReg(uint8_t addr, uint8_t port, bool enabled) {
	addr_port_t addr_port(addr, port);
	if (enabled) {
		m_tracked_regs.insert(std::make_pair(addr_port, (uint16_t) 0));
	} else {
		m_tracked_regs.erase(addr_port);
	}
}

void Device::updateTrackedRegs() {
	// send multiple register read commands
	uint16_t rd_cmd[m_tracked_regs.size()];
	{
		int i = 0;
		for (auto& kv: m_tracked_regs) {
			uint8_t addr(kv.first.first);
			uint8_t port(kv.first.second);
			rd_cmd[i++] = htobe16((CMD_READREG << 12) | ((addr & 0x3f) << 6) | (port & 0x3f));
		}
	}
	int n = ftdi_write_data(m_ftdi, (unsigned char*) rd_cmd, sizeof(rd_cmd));
	if (n != int(sizeof(rd_cmd))) {
		throw std::runtime_error("FTDI write error");
	}

	// read results
	uint16_t values_be[m_tracked_regs.size()];
	ftdi_read_data_wait(m_ftdi, (unsigned char*) &values_be, sizeof(values_be));

	// store results and invoke callback
	int i = 0;
	for (auto& kv: m_tracked_regs) {
		uint8_t addr(kv.first.first);
		uint8_t port(kv.first.second);
		uint16_t value_old = kv.second;
		uint16_t value = be16toh(values_be[i++]);
		kv.second = value;
		if (value != value_old && m_device_reg_change_cb) m_device_reg_change_cb(m_name, addr, port, value);
	}
}

void Device::setRegChangedCallback(fn_device_reg_changed_cb cb) {
	m_device_reg_change_cb = std::move(cb);
}

const std::string& Device::name() const {
	return m_name;
}
