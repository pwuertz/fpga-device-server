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

	void writeReg(uint8_t addr, uint8_t port, uint16_t value);
	void readReg(uint8_t addr, uint8_t port, uint16_t* value);
	void writeRegN(uint8_t addr, uint8_t port, const uint16_t* data_be, size_t n);
	void readRegN(uint8_t addr, uint8_t port, uint16_t* data_be, size_t n);

	void trackReg(uint8_t addr, uint8_t port, bool enabled=true);
	void updateTrackedRegs();
	void setRegChangedCallback(fn_device_reg_changed_cb cb);

private:
	const std::string m_name;
	ftdi_context* m_ftdi;
	std::map<addr_port_t, uint16_t> m_tracked_regs;
	fn_device_reg_changed_cb m_device_reg_change_cb;
};

typedef std::shared_ptr<Device> ptrDevice_t;

#endif /* DEVICES_DEVICE_H_ */
