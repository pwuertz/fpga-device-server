#include "ControlConnectionManager.h"

ControlConnectionManager::ControlConnectionManager() {
}

void ControlConnectionManager::start(ptrControlConnection_t c) {
	m_connections.insert(c);
	c->start();
}

void ControlConnectionManager::stop(ptrControlConnection_t c) {
	m_connections.erase(c);
	c->stop();
}

void ControlConnectionManager::stop_all() {
	for (auto c: m_connections) {
		c->stop();
	}
	m_connections.clear();
}

int ControlConnectionManager::numConnections() {
	return m_connections.size();
}
