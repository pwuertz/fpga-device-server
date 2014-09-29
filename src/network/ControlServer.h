#ifndef CONTROLSERVER_H_
#define CONTROLSERVER_H_

#include <boost/asio.hpp>
#include <string>
#include "ControlConnection.h"
#include "ControlConnectionManager.h"
#include "ControlHandler.h"

class ControlServer {
public:
	ControlServer(const ControlServer&) = delete;
	ControlServer& operator=(const ControlServer&) = delete;
	explicit ControlServer(int port, boost::asio::io_service& service, ControlHandler& handler);
	virtual ~ControlServer();

	void stop();

private:
	ControlHandler& m_handler;
	boost::asio::ip::tcp::acceptor m_acceptor;
	boost::asio::ip::tcp::socket m_socket;
	ControlConnectionManager m_connection_manager;
	void do_accept();
};

#endif /* CONTROLSERVER_H_ */
