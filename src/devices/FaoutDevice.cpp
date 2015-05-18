#include <chrono>
#include <thread>

#include "FaoutDevice.h"
#include "../libftdi/ftdi.h"

#define CMD_READREG 1
#define CMD_WRITEREG 2
#define CMD_READREG_N 3
#define CMD_WRITEREG_N 4


FaoutDevice::FaoutDevice(libusb_device* dev, std::string name) :
		m_name(std::move(name)),
		m_ftdi(nullptr),
		m_last_status(0xffff)
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

FaoutDevice::~FaoutDevice() {
	if (m_ftdi) {
		ftdi_set_bitmode(m_ftdi, 0xfb, BITMODE_RESET);
		ftdi_usb_close(m_ftdi);
		ftdi_free(m_ftdi);
	}
}

void FaoutDevice::writeReg(uint8_t addr, uint8_t port, uint16_t value) {
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

void FaoutDevice::readReg(uint8_t addr, uint8_t port, uint16_t* value) {
	// send register read command
	uint16_t rd_cmd[] = {
			htobe16((CMD_READREG << 12) | ((addr & 0x3f) << 6) | (port & 0x3f))
	};
	int n = ftdi_write_data(m_ftdi, (unsigned char*) rd_cmd, sizeof(rd_cmd));
	if (n != sizeof(rd_cmd)) {
		throw std::runtime_error("FTDI write error");
	}

	// read result
	uint16_t result;
	ftdi_read_data_wait(m_ftdi, (unsigned char*) &result, sizeof(uint16_t));
	*value = be16toh(result);
}

void FaoutDevice::writeRegN(uint8_t addr, uint8_t port, const uint16_t* data, size_t n) {
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
			out_buffer[2+i] = htobe16(data[n_sent+i]);
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

void FaoutDevice::readRegN(uint8_t addr, uint8_t port, uint16_t* data, size_t n) {
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
		ftdi_read_data_wait(m_ftdi, (unsigned char*) (data + n_read), sz_packet);
		n_read += n_packet;
	}

	// convert from big endian
	for (size_t i = 0; i < n; ++i) {
		data[i] = be16toh(data[i]);
	}
}

bool FaoutDevice::updateStatus() {
	uint16_t old_status = m_last_status;
	readReg(0, 1, &m_last_status);
	return m_last_status != old_status;
}

uint16_t FaoutDevice::lastStatus() {
	return m_last_status;
}

const std::string& FaoutDevice::name() const {
	return m_name;
}
