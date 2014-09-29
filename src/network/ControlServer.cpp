#include "ControlServer.h"

using boost::asio::ip::tcp;


ControlServer::ControlServer(int port, boost::asio::io_service& io_service_) :
		m_acceptor(io_service_),
		m_socket(io_service_),
		m_connection_manager()
{
	tcp::endpoint ep4(tcp::v4(), port);
	m_acceptor.open(ep4.protocol());
	m_acceptor.bind(ep4);
	m_acceptor.listen();
	std::cout << "Control server running - " <<
			ep4.address().to_string() << ":" << ep4.port() << std::endl;
	do_accept();
}

ControlServer::~ControlServer() {
}

void ControlServer::stop() {
	m_acceptor.close();
	m_connection_manager.stop_all();
}

void ControlServer::do_accept() {
	m_acceptor.async_accept(m_socket,
		[this](boost::system::error_code ec) {
			// check whether the server was stopped
			if (!m_acceptor.is_open()) return;

			if (!ec) {
				m_connection_manager.start(
					std::make_shared<ControlConnection>(
						std::move(m_socket), m_connection_manager)
				);
			}
			do_accept();
		});
}
