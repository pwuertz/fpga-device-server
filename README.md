Application for managing multiple (TU KL-like) FPGA devices via TCP.

# Build on Ubuntu, Debian(jessie), Raspbian(jessie)
Simply build fpga-device-server using cmake, CMakeLists.txt is provided. Also note that this project uses submodules which must be initialized before starting the build process (`git submodule update --init --recursive`).

Required packages:
```
apt-get install build-essential cmake pkg-config libusb-1.0-0-dev \
                libboost-system-dev libboost-chrono-dev
```
