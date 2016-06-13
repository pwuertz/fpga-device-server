Application for managing multiple (TU KL-like) FPGA devices via TCP.

Build on Ubuntu, Debian(jessie), Raspbian(jessie)
-------------------------------------------------
Simply build fpga-device-server using cmake, CMakeLists.txt is provided. Also note that this project uses submodules which must be initialized before starting the build process (`git submodule update --init --recursive`).

Required packages:
```
apt-get install build-essential cmake pkg-config libusb-1.0-0-dev \
                libmsgpack-dev libboost-system-dev libboost-chrono-dev
```

Build on Raspbian(wheezie)
-------------------------------------------------
Required packages:
```
apt-get install gcc-4.8 g++-4.8 libmsgpack-dev libboost-system1.50-dev\
                libboost-chrono1.50-dev libudev-dev
```
libusb 1.0.16 or higher is required and must be compiled manually from source. Then, the easiest way to build fpga-device-server is to statically link to the custom libusb-1.0.a on the PI.

