#ifndef FAOUTREQUESTHANDLER_H_
#define FAOUTREQUESTHANDLER_H_

#include <map>
#include <vector>
#include "network/RequestHandler.h"
#include "devices/FaoutManager.h"

#define RPC_RCODE_ERROR -1
#define RPC_RCODE_OK 0
#define RPC_RCODE_ADDED 1
#define RPC_RCODE_REMOVED 2
#define RPC_RCODE_STATUS 3

#define RPC_REPLY_VALUE(PACKER, VAL) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_OK); \
	PACKER << VAL; \
}

#define RPC_REPLY_BINARY(PACKER, PTR, N) { \
	PACKER.pack_array(2); \
	PACKER.pack_int8(RPC_RCODE_OK); \
	PACKER.pack_raw(N); \
	PACKER.pack_raw_body(PTR, N); \
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

#define RPC_EVENT_STATUS(PACKER, SERIAL, STATUS) { \
	PACKER.pack_array(3); \
	PACKER.pack_int8(RPC_RCODE_STATUS); \
	PACKER << SERIAL << STATUS; \
}


class FaoutRequestHandler : public RequestHandler {
public:
	typedef std::vector<msgpack::object> msgpack_args_t;
	typedef msgpack::packer<msgpack::sbuffer> msgpack_reply_t;
	typedef std::function<void(msgpack_args_t&, msgpack_reply_t&)> handler_func_t;

	FaoutRequestHandler(const FaoutRequestHandler&) = delete;
	FaoutRequestHandler& operator=(const FaoutRequestHandler&) = delete;
	explicit FaoutRequestHandler(FaoutManager& manager);
	virtual ~FaoutRequestHandler() {};

	virtual void handleRequest(msgpack::object& request,
			msgpack::packer<msgpack::sbuffer>& reply);

private:
	FaoutManager& m_manager;
	std::map<std::string, handler_func_t> m_functions;
};

#endif /* FAOUTREQUESTHANDLER_H_ */
