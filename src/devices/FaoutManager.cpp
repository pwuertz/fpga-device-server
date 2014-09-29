#include "FaoutManager.h"

FaoutManager::FaoutManager(boost::asio::libusb_service& service) :
	m_libusb_service(service)
{
	int faout_vid = 0x0403;
	int faout_pid = 0x6010;
	m_libusb_service.addHotplugHandler(faout_vid, faout_pid, LIBUSB_HOTPLUG_MATCH_ANY,
			[this](libusb_context* ctx, libusb_device* dev, libusb_hotplug_event ev) {
		if (ev == LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED) {
			if (!hasDevice(dev)) {
				try {
					// TODO: use serial for identifying faout devices?
					auto faout = std::make_shared<FaoutDevice>(dev);
					if (hasSerial(faout->m_serial)) {
						std::cerr << "Not adding 2nd device with identical serial: " << faout->m_serial << std::endl;
					} else {
						m_device_map.insert(std::make_pair(dev, faout->shared_from_this()));
						m_serial_map.insert(std::make_pair(faout->m_serial, faout->shared_from_this()));
						std::cout << "Added device (" << faout->m_serial << ")" << std::endl;
					}
				} catch (const std::exception& e) {
					std::cerr << "Adding device failed: " << e.what() << std::endl;
				}
			}
		} else {
			if (hasDevice(dev)) {
				std::cout << "Removed device (" << m_device_map[dev]->m_serial << ")" << std::endl;
				m_serial_map.erase(m_device_map[dev]->m_serial);
			}
			m_device_map.erase(dev);
		}
	});
}

FaoutManager::~FaoutManager() {
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
