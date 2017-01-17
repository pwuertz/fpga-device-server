# FPGA Device Server
Application for managing multiple (TU KL-like) FPGA devices via TCP.

## Compiling fpga-device-server
Get the source from the repository, and initialize the submodules:
```
git clone https://github.com/pwuertz/fpga-device-server.git
cd fpga-device-server
git submodule update --init --recursive
```

The required dependencies for the build process are easily installed on Debian/Ubuntu systems:
```
apt-get install build-essential cmake pkg-config libusb-1.0-0-dev \
                libboost-system-dev libboost-chrono-dev
```

Build the application via cmake, preferably in a separate build directory:
```
mkdir build && cd build
cmake ..
make
```

## Running fpga-device-server
For running the device server a properly configured `config.json` file must be present in the current working directory. An example configuration file is provided in the repository. Any fpga bitfiles to be programmed by the server must be readable by the process.

Also make sure that the *fpga-device-server* application has read/write permissions to the appropriate USB devices. The provided *udev* rules file `99-ftdi-tu-kl.rules` for example will set up the correct permissions for TU KL-like devices when added to the *udev* rules folder.

 On successful startup the server identifies connected USB devices and initializes the hardware according to the configuration file. Example output:
```no-highlight
$ ./fpga-device-server
Starting device-server
Listening on: 0.0.0.0:9002
New device: Digitizer2, TU KL, DIGI29LUBF
Adding DIGI29LUBF: Digitizer2
Programming DIGI29LUBF: digitizer2-firmware.bit
Finished programming DIGI29LUBF
```

## Reference client application and library

This repository includes a python reference library called `pyfpgaclient`  for interacting with the device server. The library is found in the `client/python` folder. The easiest way of using it is to install it to the local python environment:
```
python setup.py develop --user
```

The library comes with a simple PyQt based client application which is quite useful for device testing. Simply run
```
python -m pyfpgaclient.QTestApplication host port
```
which will connect to a fpga-device-server running on the specified host/port.
