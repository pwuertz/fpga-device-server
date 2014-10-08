#ifndef DEVICES_FAOUTDEVICE_H_
#define DEVICES_FAOUTDEVICE_H_

#include "../libusb_asio/libusb_service.h"

struct ftdi_context;

class FaoutDevice : public std::enable_shared_from_this<FaoutDevice> {
public:
	FaoutDevice(const FaoutDevice&) = delete;
	FaoutDevice& operator=(const FaoutDevice&) = delete;
	FaoutDevice(libusb_device* dev, std::string name);
	virtual ~FaoutDevice();
	const std::string& name() const;

	bool updateStatus();
	uint16_t lastStatus();

	void writeReg(uint8_t addr, uint16_t value);
	void readReg(uint8_t addr, uint16_t* value);
	void writeRam(const uint16_t* data, size_t n);

private:
	const std::string m_name;
	ftdi_context* m_ftdi;
	uint16_t m_last_status;
};

typedef std::shared_ptr<FaoutDevice> ptrFaoutDevice_t;

#endif /* DEVICES_FAOUTDEVICE_H_ */
