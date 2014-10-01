#ifndef CONTROLHANDLER_H_
#define CONTROLHANDLER_H_

#include <msgpack.hpp>

class RequestHandler {
public:
	RequestHandler(const RequestHandler&) = delete;
	RequestHandler& operator=(const RequestHandler&) = delete;
	explicit RequestHandler() {};
	virtual ~RequestHandler() {};

	virtual void handleRequest(msgpack::object& request, msgpack::packer<msgpack::sbuffer>& reply) = 0;
};

#endif /* CONTROLHANDLER_H_ */
