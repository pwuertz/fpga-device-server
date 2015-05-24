#include <boost/asio.hpp>
#include <iostream>
#include <execinfo.h>

#include "libusb_asio/libusb_service.h"
#include "network/Server.h"
#include "config/Config.h"
#include "devices/DeviceManager.h"
#include "DeviceRequestHandler.h"


int main() {
#ifdef VERSION_STR
	std::cout << "Starting device-server, " << VERSION_STR << std::endl << std::endl;
#else
	std::cout << "Starting device-server" << std::endl;
#endif

	try {
		// read configuration file
		Config config = Config::fromFile("config.json");

		// asio main loop
		boost::asio::io_service io_service;

		// add usb service
		boost::asio::libusb_service libusb_service(io_service);
		DeviceManager device_manager(io_service, libusb_service, config.device_descriptions);
		DeviceRequestHandler rpc_handler(device_manager);

		// add network service
		Server server(config.port, io_service, rpc_handler);

		// add handlers for FaoutManager events
		device_manager.setAddedCallback([&](const std::string& serial){
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			RPC_EVENT_ADDED(packer_out, serial);
			server.sendAll(buffer_out);
		});
		device_manager.setRemovedCallback([&](const std::string& serial){
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			RPC_EVENT_REMOVED(packer_out, serial);
			server.sendAll(buffer_out);
		});
		device_manager.setRegChangedCallback([&](const std::string& serial, uint8_t addr, uint8_t port, uint16_t value) {
            auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			RPC_EVENT_REG_CHANGED(packer_out, serial, addr, port, value);
			server.sendAll(buffer_out);
		});

		// add system signal handler
        boost::asio::signal_set signals(io_service);
        signals.add(SIGINT);
        signals.add(SIGTERM);
        signals.add(SIGQUIT);
        signals.async_wait(
			[&](boost::system::error_code, int)
			{
				std::cout << "Shutting down" << std::endl;
				device_manager.stop();
				server.stop();
				libusb_service.stop();
			}
		);

		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Unhandled exception in Event Loop: " << e.what() << std::endl;
	}
	return 0;
}
