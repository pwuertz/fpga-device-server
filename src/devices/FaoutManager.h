#ifndef DEVICES_FAOUTMANAGER_H_
#define DEVICES_FAOUTMANAGER_H_

#include <list>

#include "../libusb_asio/libusb_service.h"
#include "FaoutDevice.h"

class FaoutManager {
public:
	FaoutManager(const FaoutManager&) = delete;
	FaoutManager& operator=(const FaoutManager&) = delete;
	FaoutManager(boost::asio::libusb_service& service);
	virtual ~FaoutManager();

	void getDeviceList(std::list<std::string>& list);
	ptrFaoutDevice_t getDevice(const std::string& serial);

	bool hasDevice(libusb_device*);
	bool hasSerial(const std::string& serial);

private:
	boost::asio::libusb_service& m_libusb_service;
	std::map<libusb_device*, ptrFaoutDevice_t> m_device_map;
	std::map<const std::string, ptrFaoutDevice_t> m_serial_map;
};

#endif /* DEVICES_FAOUTMANAGER_H_ */
