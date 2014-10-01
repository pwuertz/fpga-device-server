#include "FaoutManager.h"

#include <boost/algorithm/string.hpp>
#include <chrono>


class LibUsbDevice {
public:
	LibUsbDevice(libusb_device* dev) : m_dev_handle(nullptr) {
		int r;
		// read descriptor
	    if ((r = libusb_get_device_descriptor(dev, &m_desc)) < 0) {
	    	throw std::runtime_error(libusb_error_name(r));
	    }

	    if ((r = libusb_open(dev, &m_dev_handle)) < 0) {
	    	throw std::runtime_error(libusb_error_name(r));
	    }
	}

	void getStringDescriptor(std::string& str, uint8_t index) {
		int r;
		char buffer[256];
	    if ((r = libusb_get_string_descriptor_ascii(m_dev_handle, index,
	    		(unsigned char*) buffer, sizeof(buffer))) < 0) {
	    	throw std::runtime_error(libusb_error_name(r));
	    }
	    str = std::string(buffer);
	}

	~LibUsbDevice() {
		if (!m_dev_handle) libusb_close(m_dev_handle);
	}

	libusb_device_handle* m_dev_handle;
	libusb_device_descriptor m_desc;
};

void getUsbDeviceStrings(libusb_device* dev,
		std::string& manufacturer, std::string& product, std::string& serial) {
	LibUsbDevice usbdev(dev);
	usbdev.getStringDescriptor(manufacturer, usbdev.m_desc.iManufacturer);
	usbdev.getStringDescriptor(product, usbdev.m_desc.iProduct);
	usbdev.getStringDescriptor(serial, usbdev.m_desc.iSerialNumber);
}


FaoutManager::FaoutManager(boost::asio::io_service& io_service,
		boost::asio::libusb_service& usb_service) :
	m_timer(io_service),
	m_libusb_service(usb_service),
	m_device_map(),
	m_serial_map(),
	m_device_added_cb(),
	m_device_removed_cb(),
	m_device_status_cb()
{
	// handler for added or removed faout (ftdi) devices
	int faout_vid = 0x0403;
	int faout_pid = 0x6010;
	m_libusb_service.addHotplugHandler(faout_vid, faout_pid, LIBUSB_HOTPLUG_MATCH_ANY,
			[this](libusb_context* ctx, libusb_device* dev, libusb_hotplug_event ev) {
		if (ev == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
			if (!hasDevice(dev)) {
				try {
					// get strings from usb device
					std::string manufacturer, product, serial;
					getUsbDeviceStrings(dev, manufacturer, product, serial);

					// is the ftdi device a faout device?
					std::cout << "New device: " << product << ", " << manufacturer << std::endl;
					if (!boost::algorithm::starts_with(serial, "FAOUT")) {
						std::cout << "Not identified as Faout device: " << serial << std::endl;
						return;
					}
					// make sure the serial is unique
					if (hasSerial(serial)) {
						std::cerr << "Not adding device with identical serial: " << serial << std::endl;
						return;
					}
					// add new device to manager
					std::cout << "Adding Faout device: " << serial << std::endl;
					auto faout = std::make_shared<FaoutDevice>(dev, serial);
					m_device_map.insert(std::make_pair(dev, faout->shared_from_this()));
					m_serial_map.insert(std::make_pair(faout->name(), faout->shared_from_this()));
					if (m_device_added_cb) m_device_added_cb(faout->name());

				} catch (const std::exception& e) {
					std::cerr << "Adding device failed: " << e.what() << std::endl;
				}
			}
		} else {
			if (hasDevice(dev)) {
				const std::string& serial = m_device_map[dev]->name();
				std::cout << "Removed Faout device: " << serial << std::endl;
				if (m_device_added_cb) m_device_removed_cb(serial);
				m_serial_map.erase(serial);
				m_device_map.erase(dev);
			}
		}
	});

	// start periodic status updates of registered devices
	_periodicStatusUpdates();
}

FaoutManager::~FaoutManager() {
	stop();
}

void FaoutManager::_periodicStatusUpdates() {
	// get status updates for all devices and emit callbacks
	for (auto p: m_serial_map) {
		try {
			auto device = p.second;
			if (device->updateStatus() && m_device_status_cb)
				m_device_status_cb(p.first, device->lastStatus());
		} catch (std::exception& e) {
			std::cerr << "Error updating status of " << p.first;
			std::cerr << ", " << e.what() << std::endl;
		}
	}
	// schedule next update
	m_timer.expires_from_now(std::chrono::milliseconds(FAOUT_MANAGER_UPDATE_DELAY_MS));
	m_timer.async_wait([this](const boost::system::error_code& ec) {
		if (!ec) {
			_periodicStatusUpdates();
		}
	});
}

void FaoutManager::stop() {
	m_timer.cancel();
}

bool FaoutManager::hasDevice(libusb_device* dev) {
	return m_device_map.find(dev) != m_device_map.end();
}

void FaoutManager::getDeviceList(std::list<std::string>& list) {
	for (auto elem: m_serial_map) {
		list.push_back(elem.first);
	}
}

ptrFaoutDevice_t FaoutManager::getDevice(const std::string& serial) {
	if (hasSerial(serial)) {
		return m_serial_map[serial]->shared_from_this();
	} else {
		return nullptr;
	}
}

bool FaoutManager::hasSerial(const std::string& serial) {
	return m_serial_map.find(serial) != m_serial_map.end();
}

void FaoutManager::setAddedCallback(fn_device_added_cb cb) {
	m_device_added_cb = std::move(cb);
}

void FaoutManager::setRemovedCallback(fn_device_removed_cb cb) {
	m_device_removed_cb = std::move(cb);
}

void FaoutManager::setStatusCallback(fn_device_status_cb cb) {
	m_device_status_cb = std::move(cb);
}
