#ifndef CONTROLHANDLER_H_
#define CONTROLHANDLER_H_

#include <msgpack.hpp>

class ControlHandler {
public:
	ControlHandler(const ControlHandler&) = delete;
	ControlHandler& operator=(const ControlHandler&) = delete;
	explicit ControlHandler() {};
	virtual ~ControlHandler() {};

	virtual void handleRequest(msgpack::object& request, msgpack::packer<msgpack::sbuffer>& reply) = 0;
};

#endif /* CONTROLHANDLER_H_ */
