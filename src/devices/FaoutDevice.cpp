#include <chrono>
#include <thread>

#include "FaoutDevice.h"
#include "../libftdi/ftdi.h"


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
		ftdi_usb_close(m_ftdi);
		ftdi_free(m_ftdi);
	}
}

void FaoutDevice::writeReg(uint8_t addr, uint16_t value) {
	// send register write command
	uint16_t wr_cmd[] = {htobe16((2<<8) | addr), htobe16(value)};
	int n = ftdi_write_data(m_ftdi, (unsigned char*) wr_cmd, sizeof(wr_cmd));
	if (n != sizeof(wr_cmd)) {
		throw std::runtime_error("FTDI write error");
	}
}

void FaoutDevice::readReg(uint8_t addr, uint16_t* value) {
	// send register read command
	uint16_t rd_cmd[] = {htobe16((1<<8) | addr)};
	int n = ftdi_write_data(m_ftdi, (unsigned char*) rd_cmd, sizeof(rd_cmd));
	if (n != sizeof(rd_cmd)) {
		throw std::runtime_error("FTDI write error");
	}

	// wait for the response
	int n_recv = 0;
	int n_total = sizeof(uint16_t);
	uint16_t result;
	unsigned char* p = (unsigned char*) &result;
	// loop for a maximum duration of 1s
	for (int i = 0; i < 100; ++i) {
		// try to read the remaining bytes
		n = ftdi_read_data(m_ftdi, p + n_recv, n_total - n_recv);
		if (n < 0) {
			std::cerr << "FTDI read error " << n << std::endl;
			throw std::runtime_error("FTDI read error");
		}
		n_recv += n;
		// continue reading if there are bytes left
		if (n_recv != n_total) {
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
		} else {
			break;
		}
	}
	if (n_recv != n_total) {
		std::cerr << "FTDI read timeout" << std::endl;
		throw std::runtime_error("FTDI read timeout");
	}

	*value = be16toh(result);
}

bool FaoutDevice::updateStatus() {
	uint16_t old_status = m_last_status;
	readReg(0, &m_last_status);
	return m_last_status != old_status;
}

uint16_t FaoutDevice::lastStatus() {
	return m_last_status;
}

void FaoutDevice::writeRam(const uint16_t* data, unsigned int n) {
	const unsigned int max_words = (1<<16)-1;
	uint16_t out_buffer[2 + max_words];

	// store ram ptr
	uint16_t lw, hw;
	readReg(8, &lw); readReg(9, &hw);
	uint32_t wr_ptr_before = (hw << 16) | lw;
	std::cout << "Wr ptr: " << wr_ptr_before << std::endl;

	unsigned int nwords_sent = 0;
	while (nwords_sent != n) {
		uint16_t nwords_packet = std::min(n-nwords_sent, max_words);
		int nbytes_packet = nwords_packet * sizeof(uint16_t);
		out_buffer[0] = be16toh(4 << 8);  // write ram cmd, no addr required
		out_buffer[1] = be16toh(nwords_packet);
		for (int i = 0; i < nwords_packet; ++i) {
			out_buffer[i+2] = be16toh(data[nwords_sent + i]);
		}

		int offset = 0, i=0;
		while (offset != nbytes_packet && i < 20) {
			int r = ftdi_write_data(m_ftdi, ((unsigned char*) out_buffer)+offset, nbytes_packet-offset);
			if (r < 0) {
				std::cerr << "FTDI write error:" << r << std::endl;
				throw std::runtime_error("FTDI write error");
			}
			offset += r;
			++i;
		}
		if (offset != nbytes_packet) {
			std::cerr << "FTDI write timeout" << std::endl;
			throw std::runtime_error("FTDI write timeout");
		}

		readReg(8, &lw); readReg(9, &hw);
		uint32_t wr_ptr = (hw << 16) | lw;
		std::cout << "Transferred: " << wr_ptr - wr_ptr_before << "/" << n << std::endl;

		nwords_sent += nwords_packet;
	}
}

const std::string& FaoutDevice::name() const {
	return m_name;
}
