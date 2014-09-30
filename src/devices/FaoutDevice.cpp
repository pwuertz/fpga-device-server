#include <chrono>
#include <thread>

#include "FaoutDevice.h"
#include "../libftdi/ftdi.h"


static std::string getDeviceSerial(libusb_device* dev) {
	int r;

	// get the device descriptor
	struct libusb_device_descriptor desc;
    if ((r = libusb_get_device_descriptor(dev, &desc)) < 0) {
    	throw std::runtime_error(libusb_error_name(r));
    }

    // try to open device
    libusb_device_handle* dev_handle;
    if ((r = libusb_open(dev, &dev_handle)) < 0) {
    	throw std::runtime_error(libusb_error_name(r));
    }

    // try to get device serial
    char serial[256] = "";
    if (libusb_get_string_descriptor_ascii(dev_handle, desc.iSerialNumber,
    		(unsigned char*) serial, sizeof(serial)) < 0) {
    	libusb_close(dev_handle);
    	throw std::runtime_error("libusb_get_string_descriptor_ascii failed");
    }
    libusb_close(dev_handle);

    return std::string(serial);
}


FaoutDevice::FaoutDevice(libusb_device* dev) :
		m_serial(getDeviceSerial(dev)),
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

bool FaoutDevice::writeReg(uint8_t addr, uint16_t value) {
	// send register write command
	uint16_t wr_cmd[] = {htobe16((2<<8) | addr), htobe16(value)};
	int n = ftdi_write_data(m_ftdi, (unsigned char*) wr_cmd, sizeof(wr_cmd));
	return (n == sizeof(wr_cmd));
}

bool FaoutDevice::readReg(uint8_t addr, uint16_t* value) {
	// send register read command
	uint16_t rd_cmd[] = {htobe16((1<<8) | addr)};
	int n = ftdi_write_data(m_ftdi, (unsigned char*) rd_cmd, sizeof(rd_cmd));
	if (n != sizeof(rd_cmd)) {
		return false;
	}

	// wait for the response
	int n_recv = 0;
	int n_total = sizeof(uint16_t);
	uint16_t result;
	unsigned char* p = (unsigned char*) &result;
	for (int i = 0; i < 20; ++i) {
		// try to read the remaining bytes
		n = ftdi_read_data(m_ftdi, p + n_recv, n_total - n_recv);
		if (n < 0) {
			return false;
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
		return false;
	}

	*value = be16toh(result);
	return true;
}

bool FaoutDevice::updateStatus() {
	uint16_t old_status = m_last_status;
	if (!readReg(0, &m_last_status)) {
		// TODO: handle usb device error
	}
	return m_last_status != old_status;
}

uint16_t FaoutDevice::lastStatus() {
	return m_last_status;
}

bool FaoutDevice::writeRam(const uint16_t* data, unsigned int n) {
	const unsigned int max_words = (1<<16)-1;
	uint16_t out_buffer[2 + max_words];

	unsigned int n_sent = 0;
	while (n_sent != n) {
		uint16_t n_packet = std::min(n-n_sent, max_words);
		int n_packet_bytes = n_packet * sizeof(uint16_t);
		out_buffer[0] = be16toh(4 << 8);  // write ram cmd, no addr required
		out_buffer[1] = be16toh(n_packet);
		for (int i = 0; i < n_packet; ++i) {
			out_buffer[i+2] = be16toh(data[n_sent + i]);
		}
		int r = ftdi_write_data(m_ftdi, (unsigned char*) out_buffer, n_packet_bytes);
		if (r < 0 || (r != n_packet_bytes)) {
			return false;
		}
		n_sent += n_packet;
	}
	return true;
}
