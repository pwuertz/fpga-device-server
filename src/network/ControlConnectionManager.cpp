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

void ControlConnectionManager::stopAll() {
	for (auto c: m_connections) {
		c->stop();
	}
	m_connections.clear();
}

void ControlConnectionManager::sendAll(std::shared_ptr<msgpack::sbuffer>& buffer) {
	for (auto c: m_connections) {
		c->send(buffer);
	}
}

int ControlConnectionManager::numConnections() {
	return m_connections.size();
}
