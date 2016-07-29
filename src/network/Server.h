//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#ifndef CONTROLSERVER_H_
#define CONTROLSERVER_H_

#include <boost/asio.hpp>
#include <string>

#include "ClientConnection.h"
#include "ConnectionManager.h"
#include "RequestHandler.h"

class Server {
public:
	Server(const Server&) = delete;
	Server& operator=(const Server&) = delete;
	explicit Server(int port, boost::asio::io_service& service, RequestHandler& handler);
	virtual ~Server();

	void sendAll(std::shared_ptr<msgpack::sbuffer>& buffer);
	void stop();

private:
	RequestHandler& m_handler;
	boost::asio::ip::tcp::acceptor m_acceptor;
	boost::asio::ip::tcp::socket m_socket;
	ConnectionManager m_connection_manager;
	void do_accept();
};

#endif /* CONTROLSERVER_H_ */
