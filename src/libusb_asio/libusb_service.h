#ifndef LIBUSB_HANDLER_H_
#define LIBUSB_HANDLER_H_

#include <iostream>
#include <functional>
#include <memory>
#include <boost/asio.hpp>
#include <libusb.h>

namespace boost {
namespace asio {

class libusb_service_fd_watcher;
class libusb_service_hotplug_handle;

typedef std::shared_ptr<libusb_service_fd_watcher> p_libusb_service_fd_watcher;
typedef std::function<void(libusb_context*, libusb_device*, libusb_hotplug_event)> fn_libusb_service_hotplug;

class libusb_service {
public:
	libusb_service(const libusb_service&) = delete;
	libusb_service& operator=(const libusb_service&) = delete;
	libusb_service(io_service & owner);
	virtual ~libusb_service();

	void stop();

	void addHotplugHandler(int vid, int pid, int dev_class, fn_libusb_service_hotplug cb);

protected:
	// add or remove fd watcher
	void startWatcher(int fd, short events);
	void stopWatcher(int fd);
	bool isWatching(int fd);

	// handlers for callbacks from libusb
	static void _pollfd_added_cb(int fd, short events, void *user_data);
	static void _pollfd_removed_cb(int fd, void *user_data);
	static int _hotplug_cb(libusb_context*, libusb_device*, libusb_hotplug_event, void*);

	// handle libusb events on fd activity
	void handleEvents();

	friend class libusb_service_fd_watcher;

private:
 	libusb_context* m_ctx;
 	std::map<int, p_libusb_service_fd_watcher> m_fd_watchers;
 	std::map<libusb_hotplug_callback_handle, libusb_service_hotplug_handle*> m_hotplug_cbs;
	io_service& m_io_service;
};

}
}

#endif /* LIBUSB_HANDLER_H_ */
