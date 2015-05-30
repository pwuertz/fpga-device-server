#include "DeviceRequestHandler.h"

template <int I=0, typename T>
void msgpack_parse(std::vector<msgpack::object>& args, T& value)
{
	args[I].convert(&value);
}

template <int I=0, typename U, typename... T>
void msgpack_parse(std::vector<msgpack::object>& args, U& head, T&... tail)
{
	msgpack_parse<I>(args, head);
    msgpack_parse<I+1>(args, tail...);
}

DeviceRequestHandler::DeviceRequestHandler(DeviceManager& manager) :
		RequestHandler(),
		m_manager(manager)
{
	// add handler functions for rpc commands

	m_functions["devicelist"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		std::list<std::string> devicelist;
		m_manager.getDeviceList(devicelist);
		RPC_REPLY_VALUE(reply, devicelist);
	};

	m_functions["reprogram"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			RPC_REPLY_VALUE(reply, m_manager.reprogramDevice(device));
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};

	m_functions["writereg"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			uint8_t addr = args.at(2).as<uint8_t>();
			uint8_t port = args.at(3).as<uint8_t>();
			uint16_t value = args.at(4).as<uint16_t>();
			device->writeReg(addr, port, value);
			RPC_REPLY_VALUE(reply, 0);
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};

	m_functions["readreg"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			uint8_t addr = args.at(2).as<uint8_t>();
			uint8_t port = args.at(3).as<uint8_t>();
			uint16_t value;
			device->readReg(addr, port, &value);
			RPC_REPLY_VALUE(reply, value);
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};

	m_functions["writeregn"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			uint8_t addr = args.at(2).as<uint8_t>();
			uint8_t port = args.at(3).as<uint8_t>();
			// get argument 4 as uint16_t (be) buffer
			if (args.at(4).type != msgpack::type::RAW)
				RPC_REPLY_ERROR(reply, "Invalid argument");
			uint16_t* data_be = (uint16_t*) args.at(4).via.raw.ptr;
			size_t n_words = args.at(4).via.raw.size / sizeof(uint16_t);

			device->writeRegN(addr, port, data_be, n_words);
			RPC_REPLY_VALUE(reply, 0);
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};

	m_functions["readregn"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			uint8_t addr = args.at(2).as<uint8_t>();
			uint8_t port = args.at(3).as<uint8_t>();
			uint32_t n_words = args.at(4).as<uint32_t>();
			std::vector<uint16_t> data_be(n_words);
			device->readRegN(addr, port, data_be.data(), n_words);
			RPC_REPLY_BINARY(reply, (char*) data_be.data(), n_words*sizeof(uint16_t));
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};
}

void DeviceRequestHandler::handleRequest(msgpack::object& request,
		msgpack::packer<msgpack::sbuffer>& reply)
{
	// basic protocol: request is an array of objects
	std::vector<msgpack::object> args;
	try {
		request.convert(&args);
	} catch (const std::exception& e) {
		 RPC_REPLY_ERROR(reply, "Invalid message");
		 return;
	}

	// first object is the command string
	std::string cmd;
	try {
		args.at(0).convert(&cmd);
	} catch (const std::exception& e) {
		RPC_REPLY_ERROR(reply, "Invalid message");
		return;
	}

	// check for valid command
	if (m_functions.find(cmd) == m_functions.end()) {
		RPC_REPLY_ERROR(reply, "Invalid command");
		return;
	}

	// try to call the function for the given command
	try {
		m_functions.at(cmd)(args, reply);
	} catch (const std::exception& e) {
		std::cerr << "Exception in RPC call: " << e.what() << std::endl;
		RPC_REPLY_ERROR(reply, e.what());
	}
}
