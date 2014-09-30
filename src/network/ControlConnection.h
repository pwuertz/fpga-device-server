#ifndef CONTROLCONNECTION_H_
#define CONTROLCONNECTION_H_

#include <memory>
#include <array>
#include <deque>
#include <boost/asio.hpp>
#include <msgpack.hpp>
#include "ControlHandler.h"

#define CONTROL_MSG_MAX_BYTES (10*1024*1024)

class ControlConnectionManager;

class ControlConnection
		: public std::enable_shared_from_this<ControlConnection> {
public:
	ControlConnection(const ControlConnection&) = delete;
	ControlConnection& operator=(const ControlConnection&) = delete;
	explicit ControlConnection(boost::asio::ip::tcp::socket socket,
			ControlConnectionManager& manager, ControlHandler& handler);
	virtual ~ControlConnection();

	void start();
	void stop();
	void send(std::shared_ptr<msgpack::sbuffer> buffer);

private:
	void do_read();
	void do_write();
	boost::asio::ip::tcp::socket m_socket;
	ControlConnectionManager& m_connection_manager;
	ControlHandler& m_handler;
	msgpack::unpacker m_msgbuffer_in;
	std::deque<std::shared_ptr<msgpack::sbuffer>> m_msgbuffer_out;
	size_t m_msgbuffer_out_offset;
};

typedef std::shared_ptr<ControlConnection> ptrControlConnection_t;

#endif /* CONTROLCONNECTION_H_ */
