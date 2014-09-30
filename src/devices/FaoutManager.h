#ifndef DEVICES_FAOUTMANAGER_H_
#define DEVICES_FAOUTMANAGER_H_

#include <list>
#include <boost/asio/steady_timer.hpp>

#include "../libusb_asio/libusb_service.h"
#include "FaoutDevice.h"

typedef std::function<void(const std::string&)> fn_device_added_cb;
typedef std::function<void(const std::string&)> fn_device_removed_cb;
typedef std::function<void(const std::string&, uint16_t)> fn_device_status_cb;

#define FAOUT_MANAGER_UPDATE_DELAY_MS 500

class FaoutManager {
public:
	FaoutManager(const FaoutManager&) = delete;
	FaoutManager& operator=(const FaoutManager&) = delete;
	FaoutManager(boost::asio::io_service& io_service,
			boost::asio::libusb_service& usb_service);
	virtual ~FaoutManager();
	void stop();

	void getDeviceList(std::list<std::string>& list);
	ptrFaoutDevice_t getDevice(const std::string& serial);

	bool hasDevice(libusb_device*);
	bool hasSerial(const std::string& serial);

	void setAddedCallback(fn_device_added_cb cb);
	void setRemovedCallback(fn_device_removed_cb cb);
	void setStatusCallback(fn_device_status_cb cb);

private:
	boost::asio::steady_timer m_timer;
	boost::asio::libusb_service& m_libusb_service;
	std::map<libusb_device*, ptrFaoutDevice_t> m_device_map;
	std::map<const std::string, ptrFaoutDevice_t> m_serial_map;
	fn_device_added_cb m_device_added_cb;
	fn_device_removed_cb m_device_removed_cb;
	fn_device_status_cb m_device_status_cb;

	void _periodicStatusUpdates();
};

#endif /* DEVICES_FAOUTMANAGER_H_ */
