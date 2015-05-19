#include <boost/asio.hpp>
#include <iostream>
#include <execinfo.h>

#include "libusb_asio/libusb_service.h"
#include "devices/FaoutManager.h"
#include "FaoutRequestHandler.h"
#include "network/Server.h"


int main() {
#ifdef VERSION_STR
	std::cout << "faout-server, " << VERSION_STR << std::endl;
#endif

	try {
		boost::asio::io_service io_service;

		// add usb service
		boost::asio::libusb_service libusb_service(io_service);
		FaoutManager faout_manager(io_service, libusb_service);
		FaoutRequestHandler rpc_handler(faout_manager);

		// add network service
		Server server(9001, io_service, rpc_handler);

		// add handlers for FaoutManager events
		faout_manager.setAddedCallback([&](const std::string& serial){
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			RPC_EVENT_ADDED(packer_out, serial);
			server.sendAll(buffer_out);
		});
		faout_manager.setRemovedCallback([&](const std::string& serial){
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			RPC_EVENT_REMOVED(packer_out, serial);
			server.sendAll(buffer_out);
		});
		faout_manager.setStatusCallback([&](const std::string& serial, uint16_t status) {
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			RPC_EVENT_STATUS(packer_out, serial, status);
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
				faout_manager.stop();
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
