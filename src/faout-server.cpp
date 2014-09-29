#include <boost/asio.hpp>
#include <iostream>

#include "libusb_asio/libusb_service.h"
#include "devices/FaoutManager.h"
#include "network/ControlServer.h"
#include "network/ControlHandler.h"

#define RETURN_RPC_ERROR(STR) { \
	reply.pack_array(2); \
	reply.pack_int8(-1); \
	reply << std::string(STR); \
    return; \
}

#define RETURN_RPC_VALUE(VAL) { \
	reply.pack_array(2); \
	reply.pack_int8(0); \
	reply << VAL; \
    return; \
}

class FaoutRpcHandler : public ControlHandler {
public:
	explicit FaoutRpcHandler(FaoutManager& manager) : ControlHandler(), m_manager(manager) {};
	virtual ~FaoutRpcHandler() {};
	virtual void handleRequest(msgpack::object& request, msgpack::packer<msgpack::sbuffer>& reply) {
		// convert msgpack object to array of objects
		std::vector<msgpack::object> args;
		try {
			request.convert(&args);
		} catch (std::exception& e) RETURN_RPC_ERROR("Invalid arguments");

		// try to read command
		std::string cmd;
		try {
			args.at(0).convert(&cmd);
		} catch (std::exception& e) RETURN_RPC_ERROR("Invalid arguments");

		// handle device list command
		if (cmd == "devicelist") {
			std::list<std::string> devicelist;
			m_manager.getDeviceList(devicelist);
			reply.pack_array(1+devicelist.size());
			reply.pack_int8(0);
			for (auto dev: devicelist) reply << dev;
			return;
		}

		// other commands require a device serial
		std::string serial;
		try {
			args.at(1).convert(&serial);
		} catch (std::exception& e) RETURN_RPC_ERROR("Invalid arguments");

		if (!m_manager.hasSerial(serial)) {
			RETURN_RPC_ERROR("Unknown device serial");
		}
		auto device = m_manager.getDevice(serial);

		// check which command to call on device
		if (cmd == "writereg") {
			uint8_t reg;
			uint16_t value;
			try {
				args.at(2).convert(&reg);
				args.at(3).convert(&value);
			} catch (std::exception& e) RETURN_RPC_ERROR("Invalid arguments");

			if (device->writeReg(reg, value)) {
				RETURN_RPC_VALUE(0);
			} else {
				// TODO: handle device errors in some way?
				RETURN_RPC_ERROR("Device error");
			}
		} else if (cmd == "readreg") {
			uint8_t reg;
			uint16_t value;
			try {
				args.at(2).convert(&reg);
			} catch (std::exception& e) RETURN_RPC_ERROR("Invalid arguments");

			if (device->readReg(reg, &value)) {
				RETURN_RPC_VALUE(value);
			} else {
				// TODO: handle device errors in some way?
				RETURN_RPC_ERROR("Device error");
			}
		}

		// command was not handled
		RETURN_RPC_ERROR("Unknown command");
	}
	FaoutManager& m_manager;
};


int main() {
	try {
		boost::asio::io_service io_service;

		// add usb service
		boost::asio::libusb_service libusb_service(io_service);
		FaoutManager faout_manager(libusb_service);
		FaoutRpcHandler faout_rpc_handler(faout_manager);

		// add rpc service
		ControlServer control_server(9000, io_service, faout_rpc_handler);

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
