cmake_minimum_required (VERSION 2.8)
project (fpga-device-server)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SRCS
        src/DeviceRequestHandler.cpp
        src/fpga-device-server.cpp
        src/libusb_asio/libusb_service.cpp
        src/libftdi/ftdi.c
        src/libftdi/ftdi_stream.c
        src/config/Config.cpp
        src/config/json11.cpp
        src/network/RequestHandler.cpp
        src/network/ClientConnection.cpp
        src/network/ConnectionManager.cpp
        src/network/Server.cpp
        src/devices/DeviceManager.cpp
        src/devices/Device.cpp
        src/devices/DeviceProgrammer.cpp
        src/devices/xc3sprog/bitrev.cpp
        src/devices/xc3sprog/bitfile.cpp
        src/devices/xc3sprog/iobase.cpp
        src/devices/xc3sprog/ioftdi.cpp
        src/devices/xc3sprog/jtag.cpp
        src/devices/xc3sprog/progalgxc3s.cpp
)

INCLUDE(FindPkgConfig)
pkg_check_modules(LIBUSB_1 REQUIRED libusb-1.0)
find_package(Boost COMPONENTS system REQUIRED)

add_executable(fpga-device-server ${SRCS})
include_directories(src src/msgpack-c/include ${LIBUSB_1_INCLUDE_DIRS})
target_link_libraries (fpga-device-server ${LIBUSB_1_LIBRARIES} ${Boost_SYSTEM_LIBRARY} pthread)
