#ifndef DEVICES_FAOUTDEVICE_H_
#define DEVICES_FAOUTDEVICE_H_

#include "../libusb_asio/libusb_service.h"

struct ftdi_context;

class FaoutDevice : public std::enable_shared_from_this<FaoutDevice> {
public:
	FaoutDevice(const FaoutDevice&) = delete;
	FaoutDevice& operator=(const FaoutDevice&) = delete;
	FaoutDevice(libusb_device* dev);
	virtual ~FaoutDevice();

	bool writeReg(uint8_t addr, uint16_t value);
	bool readReg(uint8_t addr, uint16_t* value);
	bool writeRam(const uint16_t* data, unsigned int n);

	const std::string m_serial;

private:
	ftdi_context* m_ftdi;
};

typedef std::shared_ptr<FaoutDevice> ptrFaoutDevice_t;

#endif /* DEVICES_FAOUTDEVICE_H_ */
