//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

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
