/*
 * ControlConnection.cpp
 *
 *  Created on: Sep 25, 2014
 *      Author: pwuertz
 */

#include "ControlConnection.h"
#include "ControlConnectionManager.h"

using boost::asio::ip::tcp;


ControlConnection::ControlConnection(boost::asio::ip::tcp::socket socket,
		ControlConnectionManager& manager) :
		m_socket(std::move(socket)),
		m_connection_manager(manager),
		m_msgbuffer_in(),
		m_msgbuffer_out()
{
	// store remote address for each client?
	//std::string remote = m_socket.remote_endpoint().address().to_string();
}

ControlConnection::~ControlConnection() {
	// TODO Auto-generated destructor stub
}

void ControlConnection::start() {
	do_read();
}

void ControlConnection::stop() {
	m_socket.close();
}

void ControlConnection::do_read() {
	// reserve buffer for incoming data
	m_msgbuffer_in.reserve_buffer(MSGPACK_UNPACKER_RESERVE_SIZE);

	// asynchronously wait for incoming data
	auto self(shared_from_this());
	auto buffer = boost::asio::buffer(m_msgbuffer_in.buffer(), m_msgbuffer_in.buffer_capacity());
	m_socket.async_read_some(buffer,
		[this, self](boost::system::error_code ec, std::size_t bytes_transferred)
		{
			if (!ec) {
				// commit received bytes
				m_msgbuffer_in.buffer_consumed(bytes_transferred);

				// forward parsed messages to handler
				msgpack::unpacked result;
				while(m_msgbuffer_in.next(&result)) {
					// handle received message
					msgpack::object object = result.get();
					std::cout << "Got: " << object << std::endl;
				}

				// close connection if the message size exceeds a certain limit
				if(m_msgbuffer_in.message_size() > CONTROL_MSG_MAX_BYTES) {
					std::cerr << "Message size exceeded, dropping client" << std::endl;
					m_connection_manager.stop(shared_from_this());
					return;
				}

				// continue waiting for incoming data
				do_read();
	        } else if (ec != boost::asio::error::operation_aborted) {
	        	m_connection_manager.stop(shared_from_this());
	        }
		});
}

void ControlConnection::do_write() {
}
