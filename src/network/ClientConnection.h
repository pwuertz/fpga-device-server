//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#ifndef CONTROLCONNECTION_H_
#define CONTROLCONNECTION_H_

#include <memory>
#include <array>
#include <deque>
#include <boost/asio.hpp>
#include <msgpack.hpp>

#include "RequestHandler.h"

#define CONTROL_MSG_MAX_BYTES (10*1024*1024)

class ConnectionManager;

class ClientConnection
		: public std::enable_shared_from_this<ClientConnection> {
public:
	ClientConnection(const ClientConnection&) = delete;
	ClientConnection& operator=(const ClientConnection&) = delete;
	explicit ClientConnection(boost::asio::ip::tcp::socket socket,
			ConnectionManager& manager, RequestHandler& handler);
	virtual ~ClientConnection();

	void start();
	void stop();
	void send(std::shared_ptr<msgpack::sbuffer> buffer);

private:
	void do_read();
	void do_write();
	boost::asio::ip::tcp::socket m_socket;
	ConnectionManager& m_connection_manager;
	RequestHandler& m_handler;
	msgpack::unpacker m_msgbuffer_in;
	std::deque<std::shared_ptr<msgpack::sbuffer>> m_msgbuffer_out;
	size_t m_msgbuffer_out_offset;
};

typedef std::shared_ptr<ClientConnection> ptrClientConnection_t;

#endif /* CONTROLCONNECTION_H_ */
