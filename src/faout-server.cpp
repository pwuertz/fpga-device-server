#include <boost/asio.hpp>
#include <iostream>
#include <execinfo.h>

#include "libusb_asio/libusb_service.h"
#include "devices/FaoutManager.h"
#include "network/RequestHandler.h"
#include "network/Server.h"

#define RPC_RCODE_ERROR -1
#define RPC_RCODE_OK 0
#define RPC_RCODE_ADDED 1
#define RPC_RCODE_REMOVED 2
#define RPC_RCODE_STATUS 3

#define RETURN_RPC_VALUE(PACKER, VAL) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_OK); \
	PACKER << VAL; \
    return; \
}

#define RETURN_RPC_ERROR(PACKER, STR) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_ERROR); \
	PACKER << std::string(STR); \
    return; \
}

class FaoutRpcHandler : public RequestHandler {
public:
	explicit FaoutRpcHandler(FaoutManager& manager) : RequestHandler(), m_manager(manager) {};
	virtual ~FaoutRpcHandler() {};
	virtual void handleRequest(msgpack::object& request, msgpack::packer<msgpack::sbuffer>& reply) {
		// convert msgpack object to array of objects
		std::vector<msgpack::object> args;
		try {
			request.convert(&args);
		} catch (std::exception& e) RETURN_RPC_ERROR(reply, "Invalid arguments");

		// try to read command
		std::string cmd;
		try {
			args.at(0).convert(&cmd);
		} catch (std::exception& e) RETURN_RPC_ERROR(reply, "Invalid arguments");

		// handle device list command
		if (cmd == "devicelist") {
			std::list<std::string> devicelist;
			m_manager.getDeviceList(devicelist);
			RETURN_RPC_VALUE(reply, devicelist);
		}

		// other commands require a device serial
		std::string serial;
		try {
			args.at(1).convert(&serial);
		} catch (std::exception& e) RETURN_RPC_ERROR(reply, "Invalid arguments");

		if (!m_manager.hasSerial(serial)) {
			RETURN_RPC_ERROR(reply, "Unknown device serial");
		}
		auto device = m_manager.getDevice(serial);

		// check which command to call on device
		if (cmd == "status") {
			RETURN_RPC_VALUE(reply, device->lastStatus());
		} else if (cmd == "writereg") {
			uint8_t reg;
			uint16_t value;
			try {
				args.at(2).convert(&reg);
				args.at(3).convert(&value);
			} catch (std::exception& e) RETURN_RPC_ERROR(reply, "Invalid arguments");

			if (device->writeReg(reg, value)) {
				RETURN_RPC_VALUE(reply, 0);
			} else {
				// TODO: handle device errors in some way?
				std::cerr << "Error writing reg " << int(reg) << std::endl;
				RETURN_RPC_ERROR(reply, "Device error");
			}
		} else if (cmd == "readreg") {
			uint8_t reg;
			uint16_t value;
			try {
				args.at(2).convert(&reg);
			} catch (std::exception& e) RETURN_RPC_ERROR(reply, "Invalid arguments");

			if (device->readReg(reg, &value)) {
				RETURN_RPC_VALUE(reply, value);
			} else {
				// TODO: handle device errors in some way?
				std::cerr << "Error reading reg (" << int(reg) << ")" << std::endl;
				RETURN_RPC_ERROR(reply, "Device error");
			}
		} else if (cmd == "writeram") {
			std::vector<uint16_t> sequence;
			try {
				args.at(2).convert(&sequence);
			} catch (std::exception& e) RETURN_RPC_ERROR(reply, "Invalid arguments");

			if (device->writeRam(sequence.data(), sequence.size())) {
				RETURN_RPC_VALUE(reply, 0);
			} else {
				// TODO: handle device errors in some way?
				std::cerr << "Error writing ram" << std::endl;
				RETURN_RPC_ERROR(reply, "Device error");
			}
		}

		// command was not handled
		RETURN_RPC_ERROR(reply, "Unknown command");
	}
	FaoutManager& m_manager;
};


int main() {
	try {
		boost::asio::io_service io_service;

		// add usb service
		boost::asio::libusb_service libusb_service(io_service);
		FaoutManager faout_manager(io_service, libusb_service);
		FaoutRpcHandler faout_rpc_handler(faout_manager);

		// add network service
		Server server(9000, io_service, faout_rpc_handler);

		// add faout event handlers
		faout_manager.setAddedCallback([&](const std::string& serial){
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			packer_out.pack_array(2);
			packer_out.pack_int8(RPC_RCODE_ADDED);
			packer_out << serial;
			server.sendAll(buffer_out);
		});
		faout_manager.setRemovedCallback([&](const std::string& serial){
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			packer_out.pack_array(2);
			packer_out.pack_int8(RPC_RCODE_REMOVED);
			packer_out << serial;
			server.sendAll(buffer_out);
		});
		faout_manager.setStatusCallback([&](const std::string& serial, uint16_t status) {
			auto buffer_out = std::make_shared<msgpack::sbuffer>();
			msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
			packer_out.pack_array(3);
			packer_out.pack_int8(RPC_RCODE_STATUS);
			packer_out << serial;
			packer_out << status;
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
