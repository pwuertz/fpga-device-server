#ifndef DEVICES_DEVICEMANAGER_H_
#define DEVICES_DEVICEMANAGER_H_

#include <list>
#include <boost/asio/steady_timer.hpp>

#include "../libusb_asio/libusb_service.h"
#include "Device.h"

typedef std::function<void(const std::string&)> fn_device_added_cb;
typedef std::function<void(const std::string&)> fn_device_removed_cb;

#define DEVICE_MANAGER_UPDATE_DELAY_MS 500

void getUsbDeviceStrings(libusb_device* dev,
		std::string& manufacturer, std::string& product, std::string& serial);

class DeviceManager {
public:
	struct device_description_t {
		std::string name;
		std::string serial_prefix;
		std::string fname_bitfile;
		std::list<Device::addr_port_t> watchlist;
	};
	typedef std::list<device_description_t> device_descriptions_t;

	DeviceManager(const DeviceManager&) = delete;
	DeviceManager& operator=(const DeviceManager&) = delete;
	DeviceManager(boost::asio::io_service& io_service,
			boost::asio::libusb_service& usb_service,
			device_descriptions_t device_descriptions);
	virtual ~DeviceManager();
	void stop();

	void getDeviceList(std::list<std::string>& list);
	ptrDevice_t getDevice(const std::string& serial);

	bool hasDevice(libusb_device*);
	bool hasSerial(const std::string& serial);

	void setAddedCallback(fn_device_added_cb cb);
	void setRemovedCallback(fn_device_removed_cb cb);
	void setRegChangedCallback(fn_device_reg_changed_cb cb);

private:
	boost::asio::steady_timer m_timer;
	boost::asio::libusb_service& m_libusb_service;
	std::map<libusb_device*, ptrDevice_t> m_device_map;
	device_descriptions_t m_device_descriptions;
	std::map<const std::string, ptrDevice_t> m_serial_map;
	fn_device_added_cb m_device_added_cb;
	fn_device_removed_cb m_device_removed_cb;
	fn_device_reg_changed_cb m_device_reg_change_cb;

	void _periodicRegisterUpdates();
};

#endif /* DEVICES_DEVICEMANAGER_H_ */
