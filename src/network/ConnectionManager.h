//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#ifndef CONTROLCONNECTIONMANAGER_H_
#define CONTROLCONNECTIONMANAGER_H_

#include <msgpack.hpp>

#include "ClientConnection.h"

class ConnectionManager {
public:
	ConnectionManager(const ConnectionManager&) = delete;
	ConnectionManager& operator=(const ConnectionManager&) = delete;
	ConnectionManager();

	void start(ptrClientConnection_t c);
	void stop(ptrClientConnection_t c);
	void stopAll();

	void sendAll(std::shared_ptr<msgpack::sbuffer>& buffer);
	int numConnections();

private:
	std::set<ptrClientConnection_t> m_connections;
};

#endif /* CONTROLCONNECTIONMANAGER_H_ */
