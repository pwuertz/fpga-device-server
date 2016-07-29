//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#ifndef DEVICEREQUESTHANDLER_H_
#define DEVICEREQUESTHANDLER_H_

#include <map>
#include <vector>

#include "devices/DeviceManager.h"
#include "network/RequestHandler.h"

#define RPC_RCODE_ERROR -1
#define RPC_RCODE_OK 0
#define RPC_RCODE_ADDED 1
#define RPC_RCODE_REMOVED 2
#define RPC_RCODE_REG_CHANGED 3

#define RPC_REPLY_VALUE(PACKER, VAL) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_OK); \
	PACKER << VAL; \
}

#define RPC_REPLY_BINARY(PACKER, PTR, N) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_OK); \
	PACKER.pack_bin(N); \
	PACKER.pack_bin_body(PTR, N); \
}

#define RPC_REPLY_ERROR(PACKER, STR) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_ERROR); \
	PACKER << std::string(STR); \
}

#define RPC_EVENT_ADDED(PACKER, SERIAL) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_ADDED); \
	PACKER << SERIAL; \
}

#define RPC_EVENT_REMOVED(PACKER, SERIAL) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_REMOVED); \
	PACKER << SERIAL; \
}

#define RPC_EVENT_REG_CHANGED(PACKER, SERIAL, ADDR, PORT, VALUE) { \
	PACKER.pack_array(5); \
	PACKER.pack_int8(RPC_RCODE_REG_CHANGED); \
	PACKER << SERIAL << ADDR << PORT << VALUE; \
}


class DeviceRequestHandler : public RequestHandler {
public:
	typedef std::vector<msgpack::object> msgpack_args_t;
	typedef msgpack::packer<msgpack::sbuffer> msgpack_reply_t;
	typedef std::function<void(msgpack_args_t&, msgpack_reply_t&)> handler_func_t;

	DeviceRequestHandler(const DeviceRequestHandler&) = delete;
	DeviceRequestHandler& operator=(const DeviceRequestHandler&) = delete;
	explicit DeviceRequestHandler(DeviceManager& manager);
	virtual ~DeviceRequestHandler() {};

	virtual void handleRequest(msgpack::object& request,
			msgpack::packer<msgpack::sbuffer>& reply);

private:
	DeviceManager& m_manager;
	std::map<std::string, handler_func_t> m_functions;
};

#endif /* DEVICEREQUESTHANDLER_H_ */
