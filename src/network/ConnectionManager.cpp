//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#include "ConnectionManager.h"

ConnectionManager::ConnectionManager() {
}

void ConnectionManager::start(ptrClientConnection_t c) {
	m_connections.insert(c);
	c->start();
}

void ConnectionManager::stop(ptrClientConnection_t c) {
	m_connections.erase(c);
	c->stop();
}

void ConnectionManager::stopAll() {
	for (auto c: m_connections) {
		c->stop();
	}
	m_connections.clear();
}

void ConnectionManager::sendAll(std::shared_ptr<msgpack::sbuffer>& buffer) {
	for (auto c: m_connections) {
		c->send(buffer);
	}
}

int ConnectionManager::numConnections() {
	return m_connections.size();
}
