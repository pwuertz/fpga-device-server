#include "FaoutRequestHandler.h"

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

FaoutRequestHandler::FaoutRequestHandler(FaoutManager& manager) :
		RequestHandler(),
		m_manager(manager)
{
	// add handler functions for rpc commands

	m_functions["devicelist"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		std::list<std::string> devicelist;
		m_manager.getDeviceList(devicelist);
		RPC_REPLY_VALUE(reply, devicelist);
	};

	m_functions["status"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			RPC_REPLY_VALUE(reply, device->lastStatus());
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};

	m_functions["writereg"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			uint8_t reg = args.at(2).as<uint8_t>();
			uint16_t value = args.at(3).as<uint16_t>();
			device->writeReg(reg, value);
			RPC_REPLY_VALUE(reply, 0);
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};

	m_functions["readreg"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			uint8_t reg = args.at(2).as<uint8_t>();
			uint16_t value;
			device->readReg(reg, &value);
			RPC_REPLY_VALUE(reply, value);
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};

	m_functions["writeram"] = [&](msgpack_args_t& args, msgpack_reply_t& reply) {
		auto device = m_manager.getDevice(args.at(1).as<std::string>());
		if (device) {
			std::vector<uint16_t> sequence;
			args.at(2).convert(&sequence);
			device->writeRam(sequence.data(), sequence.size());
			RPC_REPLY_VALUE(reply, 0);
		} else {
			RPC_REPLY_ERROR(reply, "Unknown device");
		}
	};
}

void FaoutRequestHandler::handleRequest(msgpack::object& request,
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
