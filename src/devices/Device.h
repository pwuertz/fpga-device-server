//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#ifndef DEVICES_DEVICE_H_
#define DEVICES_DEVICE_H_

#include "../libusb_asio/libusb_service.h"

struct ftdi_context;

typedef std::function<void(const std::string&, uint8_t, uint8_t, uint16_t)> fn_device_reg_changed_cb;

class Device : public std::enable_shared_from_this<Device> {
public:
	typedef std::pair<uint8_t, uint8_t> addr_port_t;

	Device(const Device&) = delete;
	Device& operator=(const Device&) = delete;
	Device(libusb_device* dev, std::string name);
	virtual ~Device();
	const std::string& name() const;
	void open();
	void close();
	bool isOpen();

	libusb_device* libusbDevice();

	void writeRaw(const uint8_t* data, const size_t n);
	void readRaw(uint8_t* data, const size_t n);
	void writeReg(uint8_t addr, uint8_t port, uint16_t value);
	void readReg(uint8_t addr, uint8_t port, uint16_t* value);
	void writeRegN(uint8_t addr, uint8_t port, const uint16_t* data_be, size_t n);
	void readRegN(uint8_t addr, uint8_t port, uint16_t* data_be, size_t n);

	void trackReg(uint8_t addr, uint8_t port, bool enabled=true);
	void updateTrackedRegs();
	void setRegChangedCallback(fn_device_reg_changed_cb cb);

private:
	const std::string m_name;
	libusb_device* m_dev;
	ftdi_context* m_ftdi;
	std::map<addr_port_t, uint16_t> m_tracked_regs;
	fn_device_reg_changed_cb m_device_reg_change_cb;
};

typedef std::shared_ptr<Device> ptrDevice_t;

#endif /* DEVICES_DEVICE_H_ */
