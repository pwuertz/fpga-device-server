#include <boost/asio.hpp>
#include <iostream>

#include "libusb_asio/libusb_service.h"
#include "devices/FaoutManager.h"
#include "network/ControlServer.h"

int main() {
	try {
		boost::asio::io_service io_service;

		// add usb service
		boost::asio::libusb_service libusb_service(io_service);
		FaoutManager faout_manager(libusb_service);

		// add rpc service
		ControlServer control_server(9000, io_service);

		// add system signal handler
		boost::asio::signal_set signals(io_service);
		signals.add(SIGINT);
		signals.add(SIGTERM);
		signals.add(SIGQUIT);
		signals.async_wait(
			[&](boost::system::error_code, int)
			{
				std::cout << "Shutting down" << std::endl;
				libusb_service.stop();
				control_server.stop();
			}
		);

		io_service.run();
	} catch (std::exception& e) {
		std::cerr << "Exception in Event Loop: " << e.what() << std::endl;
	}
	return 0;
}
