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
		ControlConnectionManager& manager, ControlHandler& handler) :
		m_socket(std::move(socket)),
		m_connection_manager(manager),
		m_handler(handler),
		m_msgbuffer_in(),
		m_msgbuffer_out(),
		m_msgbuffer_out_offset(0)
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

void ControlConnection::send(std::shared_ptr<msgpack::sbuffer> buffer) {
	// if there is no write in progress, schedule do_write() call
	if (m_msgbuffer_out.empty()) {
		auto self(shared_from_this());
		m_socket.get_io_service().post([this, self](){
			do_write();
		});
	}
	// add buffer for data to send
	m_msgbuffer_out.emplace_back(buffer);
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
				try {
					while(m_msgbuffer_in.next(&result)) {
						// handle received message
						msgpack::object object = result.get();
						auto buffer_out = std::make_shared<msgpack::sbuffer>();
						msgpack::packer<msgpack::sbuffer> packer_out(buffer_out.get());
						m_handler.handleRequest(object, packer_out);
						send(buffer_out);
					}
				} catch (msgpack::unpack_error& e) {
					std::cerr << "MsgPack exception: " << e.what() << std::endl;
					m_connection_manager.stop(shared_from_this());
					return;
				} catch (std::exception& e) {
					std::cerr << "Request handling exception: " << e.what() << std::endl;
					m_connection_manager.stop(shared_from_this());
					return;
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
	if (m_msgbuffer_out.empty()) return;

	auto packet = m_msgbuffer_out.front();

	auto self(shared_from_this());
	auto buffer = boost::asio::buffer(packet->data() + m_msgbuffer_out_offset, packet->size() - m_msgbuffer_out_offset);
	m_socket.async_write_some(buffer,
		[this, self](boost::system::error_code ec, std::size_t bytes_transferred)
		{
			if (!ec) {
				auto packet = m_msgbuffer_out.front();
				m_msgbuffer_out_offset += bytes_transferred;

				if (m_msgbuffer_out_offset != packet->size()) {
					// still bytes left to send
					do_write();
				} else {
					// all bytes sent, remove packet
					m_msgbuffer_out.pop_front();
					m_msgbuffer_out_offset = 0;
					// more packets to send?
					if (!m_msgbuffer_out.empty()) {
						do_write();
					}
				}
			} else if (ec != boost::asio::error::operation_aborted) {
				m_connection_manager.stop(shared_from_this());
			}
		});
}
