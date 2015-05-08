#include "../libusb_asio/libusb_service.h"

namespace boost {
namespace asio {

class libusb_service_fd_watcher :
		public std::enable_shared_from_this<libusb_service_fd_watcher> {
public:
	libusb_service_fd_watcher(posix::stream_descriptor desc, libusb_service& serivce) :
		m_descriptor(std::move(desc)), m_libusb_service(serivce) {
	}

	~libusb_service_fd_watcher() {
		stop();
	}

	void start(short events) {
		if (events & POLLIN) do_read();
		if (events & POLLOUT) do_write();
	}

	void stop() {
		// cancel operations and release fd because
		// this watcher is not supposed to claim / close it
		if (m_descriptor.is_open()) {
			m_descriptor.cancel();
			m_descriptor.release();
		}
	}

private:
	void do_read() {
		auto self(shared_from_this());
		m_descriptor.async_read_some(null_buffers(),
			[this, self](boost::system::error_code ec, std::size_t)
			{
				if (!ec) {
					m_libusb_service.handleEvents();
					if (m_libusb_service.isWatching(m_descriptor.native_handle()))
						do_read();
				} else if (ec != boost::asio::error::operation_aborted) {
					m_libusb_service.handleEvents();
					if (m_libusb_service.isWatching(m_descriptor.native_handle())) {
						std::cerr << "libusb_service_fd_watcher: error occurred, but still supposed to watch fd?" << std::endl;
					}
		        }
			});
	}

	void do_write() {
		auto self(shared_from_this());
		m_descriptor.async_write_some(null_buffers(),
			[this, self](boost::system::error_code ec, std::size_t)
			{
				if (!ec) {
					m_libusb_service.handleEvents();
					if (m_libusb_service.isWatching(m_descriptor.native_handle()))
						do_write();
				} else if (ec != boost::asio::error::operation_aborted) {
					m_libusb_service.handleEvents();
					if (m_libusb_service.isWatching(m_descriptor.native_handle())) {
						std::cerr << "libusb_service_fd_watcher: error occurred, but still supposed to watch fd?" << std::endl;
					}
		        }
			});
	}

	posix::stream_descriptor m_descriptor;
	libusb_service& m_libusb_service;
};


class libusb_service_hotplug_handle {
public:
	libusb_service_hotplug_handle(const libusb_service_hotplug_handle&) = delete;
	libusb_service_hotplug_handle& operator=(const libusb_service_hotplug_handle&) = delete;

	libusb_service_hotplug_handle(libusb_service& service,
			fn_libusb_service_hotplug callback) :
				m_service(service),
				m_callback(std::move(callback)) {}

	libusb_service& m_service;
	fn_libusb_service_hotplug m_callback;
};


libusb_service::libusb_service(io_service& owner) :
			m_ctx(nullptr),
			m_fd_watchers(),
			m_hotplug_cbs(),
			m_io_service(owner)
{
	// initialize libusb
	libusb_init(&m_ctx);
	if (!m_ctx) {
		throw std::runtime_error("libusb_init failed");
	}

	// add initial list of fds from libusb
	const libusb_pollfd **pollfd_list = libusb_get_pollfds(m_ctx);
	const libusb_pollfd **pollfd_itr = pollfd_list;
	while (*pollfd_itr) {
		startWatcher((*pollfd_itr)->fd, (*pollfd_itr)->events);
		pollfd_itr++;
	}
	free(pollfd_list);

	// add notifiers for adding/removing libusb descriptors
	libusb_set_pollfd_notifiers(m_ctx, _pollfd_added_cb, _pollfd_removed_cb, this);
}

libusb_service::~libusb_service() {
	if (m_ctx) {
		// unregister callbacks
		libusb_set_pollfd_notifiers(m_ctx, nullptr, nullptr, nullptr);
		for (auto elem: m_hotplug_cbs) {
			libusb_hotplug_deregister_callback(m_ctx, elem.first);
			delete elem.second;
		}
		// stop all fd watchers
		stop();
		// libusb cleanup
		libusb_exit(m_ctx);
		m_ctx = nullptr;
	}
}

void libusb_service::startWatcher(int fd, short events) {
	if (isWatching(fd)) {
		std::cerr << "libusb_service: already watching " << fd << std::endl;
		return;
	}

	p_libusb_service_fd_watcher watcher = std::make_shared<libusb_service_fd_watcher>(
			posix::stream_descriptor(m_io_service, fd), *this);
	m_fd_watchers.insert(std::make_pair(fd, watcher));
	watcher->start(events);
}

void libusb_service::stopWatcher(int fd) {
	if (!isWatching(fd)) return;

	p_libusb_service_fd_watcher tmp = m_fd_watchers[fd]->shared_from_this();
	m_fd_watchers.erase(fd);
	tmp->stop();
}

bool libusb_service::isWatching(int fd) {
	return m_fd_watchers.find(fd) != m_fd_watchers.end();
}

void libusb_service::stop() {
	for (auto elem: m_fd_watchers) {
		elem.second->stop();
	}
	m_fd_watchers.clear();
}

void libusb_service::_pollfd_added_cb(int fd, short events, void* user_data) {
	static_cast<libusb_service*>(user_data)->startWatcher(fd, events);
}

void libusb_service::_pollfd_removed_cb(int fd, void* user_data) {
	static_cast<libusb_service*>(user_data)->stopWatcher(fd);
}

void libusb_service::addHotplugHandler(int vid, int pid, int dev_class,
		fn_libusb_service_hotplug callback) {

	// create a handler object containing the callback
	auto service_hnd = new libusb_service_hotplug_handle(*this, std::move(callback));
	libusb_hotplug_callback_handle libusb_hnd;

	// register callback
	int events = LIBUSB_HOTPLUG_EVENT_DEVICE_ARRIVED | LIBUSB_HOTPLUG_EVENT_DEVICE_LEFT;
	int r = libusb_hotplug_register_callback(m_ctx, (libusb_hotplug_event) events,
			LIBUSB_HOTPLUG_ENUMERATE,
			vid, pid, dev_class,
			&_hotplug_cb, service_hnd, &libusb_hnd);
	if (r != LIBUSB_SUCCESS) {
		delete service_hnd;
	    throw std::runtime_error("libusb_hotplug_register_callback failed");
	}

	// store callback handler object and the corresponding libusb handle
	m_hotplug_cbs.insert(std::make_pair(libusb_hnd, service_hnd));
}

int libusb_service::_hotplug_cb(libusb_context* ctx, libusb_device* device,
		libusb_hotplug_event event, void* user_data) {
	static_cast<libusb_service_hotplug_handle*>(user_data)->m_callback(ctx, device, event);
	return 0;
}

void libusb_service::handleEvents() {
	timeval tv = {0, 0};
	libusb_handle_events_timeout_completed(m_ctx, &tv, nullptr);
}

}
}
