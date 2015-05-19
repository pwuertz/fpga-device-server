#include "Server.h"

using boost::asio::ip::tcp;


Server::Server(int port, boost::asio::io_service& service, RequestHandler& handler) :
		m_handler(handler),
		m_acceptor(service),
		m_socket(service),
		m_connection_manager()
{
	tcp::endpoint ep4(tcp::v4(), port);
	m_acceptor.open(ep4.protocol());
	m_acceptor.set_option(tcp::acceptor::reuse_address(true));
	m_acceptor.bind(ep4);
	m_acceptor.listen();
	std::cout << "Listening on: " <<
			ep4.address().to_string() << ":" << ep4.port() << std::endl;
	do_accept();
}

Server::~Server() {
}

void Server::stop() {
	m_acceptor.close();
	m_connection_manager.stopAll();
}

void Server::do_accept() {
	m_acceptor.async_accept(m_socket,
		[this](boost::system::error_code ec) {
			// check whether the server was stopped
			if (!m_acceptor.is_open()) return;

			if (!ec) {
				m_connection_manager.start(
					std::make_shared<ClientConnection>(
						std::move(m_socket), m_connection_manager, m_handler)
				);
			}
			do_accept();
		});
}

void Server::sendAll(std::shared_ptr<msgpack::sbuffer>& buffer) {
	m_connection_manager.sendAll(buffer);
}
