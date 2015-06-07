#include "DeviceManager.h"
#include "DeviceProgrammer.h"
#include <boost/algorithm/string.hpp>
#include <chrono>


DeviceManager::device_descriptions_t::const_iterator find_description(const std::string& serial, const DeviceManager::device_descriptions_t& descriptions) {
	for (auto it = descriptions.cbegin(); it != descriptions.cend(); ++it)
		if (boost::algorithm::starts_with(serial, it->serial_prefix))
			return it;
	return descriptions.cend();
}


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


DeviceManager::DeviceManager(boost::asio::io_service& io_service,
		boost::asio::libusb_service& usb_service,
		device_descriptions_t device_descriptions) :
	m_timer(io_service),
	m_libusb_service(usb_service),
	m_device_map(),
	m_device_descriptions(std::move(device_descriptions)),
	m_serial_map(),
	m_device_added_cb(),
	m_device_removed_cb(),
	m_device_reg_change_cb()
{
	// handler for added or removed FTDI devices
	int faout_vid = 0x0403;
	int faout_pid = 0x6010;
	m_libusb_service.addHotplugHandler(faout_vid, faout_pid, LIBUSB_HOTPLUG_MATCH_ANY,
			[this](libusb_context* ctx, libusb_device* dev, libusb_hotplug_event ev) {
		if (ev == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
			// filter out spurious events just in case
			if (hasDevice(dev))
				return;

			try {
				// get strings from usb device
				std::string manufacturer, product, serial;
				getUsbDeviceStrings(dev, manufacturer, product, serial);
				std::cout << "New device: " << product << ", " << manufacturer << ", " << serial << std::endl;

				// make sure the serial is unique
				if (hasSerial(serial)) {
					std::cerr << "Not adding device with duplicate serial=" << serial << std::endl;
					return;
				}

				// search device description based on serial
				auto desc = find_description(serial, m_device_descriptions);
				if (desc == m_device_descriptions.cend()) {
					std::cout << "Ignoring device (serial=" << serial << ")" << std::endl;
					return;
				}

				// create new device, this initialization process may also bring back stalled devices
				std::cout << "Adding " << serial << ": " << desc->name << std::endl;
				auto device = std::make_shared<Device>(dev, serial);

				// program the device if bitfile is defined
				reprogramDevice(device);

				// add new device to manager
				m_device_map.insert(std::make_pair(dev, device->shared_from_this()));
				m_serial_map.insert(std::make_pair(device->name(), device->shared_from_this()));

				// emit added callback
				if (m_device_added_cb) m_device_added_cb(device->name());  // TODO: post callback to asio loop?

				// setup register tracking information and set callback
				for (auto& addr_port: desc->watchlist)
					device->trackReg(addr_port.first, addr_port.second);
				device->setRegChangedCallback([this](const std::string& serial, uint8_t addr, uint8_t port, uint16_t value) {
					// TODO: post callback to asio loop?
					if (m_device_reg_change_cb) m_device_reg_change_cb(serial, addr, port, value);
				});

			} catch (const std::exception& e) {
				std::cerr << "Adding device failed: " << e.what() << std::endl;
			}
		} else {
			if (hasDevice(dev)) {
				const std::string& serial = m_device_map[dev]->name();
				_removeDevice(serial);
			}
		}
	});

	// start periodic status updates of registered devices
	_periodicRegisterUpdates();
}

DeviceManager::~DeviceManager() {
	stop();
}

void DeviceManager::_removeDevice(const std::string& serial) {
	if (hasSerial(serial)) {
		std::cout << "Removed device: serial=" << serial << std::endl;
		try {
			if (m_device_added_cb) m_device_removed_cb(serial);
		} catch (const std::exception& e) {
			std::cerr << "Exception in device remove callback: " << e.what() << std::endl;
		}
		libusb_device* dev = m_serial_map[serial]->libusbDevice();
		m_serial_map.erase(serial);
		m_device_map.erase(dev);
	}
}

void DeviceManager::_periodicRegisterUpdates() {
	// poll tracked registers for all devices and emit callbacks
	for (auto p: m_serial_map) {
		try {
			auto device = p.second;
			device->updateTrackedRegs();
		} catch (std::exception& e) {
			std::cerr << "Error polling registers of " << p.first;
			std::cerr << ", " << e.what() << std::endl;
			_removeDevice(p.second->name());
		}
	}
	// schedule next update
	m_timer.expires_from_now(std::chrono::milliseconds(DEVICE_MANAGER_UPDATE_DELAY_MS));
	m_timer.async_wait([this](const boost::system::error_code& ec) {
		if (!ec) {
			_periodicRegisterUpdates();
		}
	});
}

bool DeviceManager::reprogramDevice(const std::string& serial) {
	return reprogramDevice(getDevice(serial));
}

bool DeviceManager::reprogramDevice(ptrDevice_t device) {
	// check for device, description, and bitfile definition
	if (!device)
		return false;
	auto desc = find_description(device->name(), m_device_descriptions);
	if (desc == m_device_descriptions.cend())
		return false;
	if (desc->fname_bitfile.empty())
		return false;
	std::cout << "Programming " << device->name() << ": " << desc->fname_bitfile << std::endl;

	// release the usb device and reprogram before claiming it again
	libusb_device* dev = device->libusbDevice();
	device->close();
	try {
		DeviceProgrammer programmer(dev);
		programmer.program(desc->fname_bitfile);
	} catch (...) {
		device->open();
		throw;
	}
	device->open();
	return true;
}

void DeviceManager::stop() {
	m_timer.cancel();
}

bool DeviceManager::hasDevice(libusb_device* dev) {
	return m_device_map.find(dev) != m_device_map.end();
}

void DeviceManager::getDeviceList(std::list<std::string>& list) {
	for (auto elem: m_serial_map) {
		list.push_back(elem.first);
	}
}

ptrDevice_t DeviceManager::getDevice(const std::string& serial) {
	if (hasSerial(serial)) {
		return m_serial_map[serial]->shared_from_this();
	} else {
		return nullptr;
	}
}

bool DeviceManager::hasSerial(const std::string& serial) {
	return m_serial_map.find(serial) != m_serial_map.end();
}

void DeviceManager::setAddedCallback(fn_device_added_cb cb) {
	m_device_added_cb = std::move(cb);
}

void DeviceManager::setRemovedCallback(fn_device_removed_cb cb) {
	m_device_removed_cb = std::move(cb);
}

void DeviceManager::setRegChangedCallback(fn_device_reg_changed_cb cb) {
	m_device_reg_change_cb = std::move(cb);
}
