#ifndef CONTROLCONNECTIONMANAGER_H_
#define CONTROLCONNECTIONMANAGER_H_

#include "ControlConnection.h"

class ControlConnectionManager {
public:
	ControlConnectionManager(const ControlConnectionManager&) = delete;
	ControlConnectionManager& operator=(const ControlConnectionManager&) = delete;
	ControlConnectionManager();
	void start(ptrControlConnection_t c);
	void stop(ptrControlConnection_t c);
	void stop_all();
	int numConnections();

private:
	std::set<ptrControlConnection_t> m_connections;
};

#endif /* CONTROLCONNECTIONMANAGER_H_ */
