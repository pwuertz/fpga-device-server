#include "DeviceProgrammer.h"
#include "xc3sprog/bitfile.h"
#include "xc3sprog/progalgxc3s.h"
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdexcept>

const int XC6SLX9_IRLEN = 6;
const int XC6SLX9_IDCMD = 0x9;

DeviceProgrammer::DeviceProgrammer(libusb_device* dev) : m_ioftdi(dev, INTERFACE_B), m_jtag(&m_ioftdi) {
	// check jtag chain
	int n = m_jtag.getChain();
	if (n != 1)
		throw std::runtime_error("Error in Jtag chain");
	if (m_jtag.getDeviceID(0) != 0x24001093)
		throw std::runtime_error("Unexpected Jtag device ID");

	// prepare jtag interface
	m_jtag.setDeviceIRLength(0, XC6SLX9_IRLEN);
	m_jtag.setTapState(Jtag::TEST_LOGIC_RESET);
	m_jtag.selectDevice(0);
}

DeviceProgrammer::~DeviceProgrammer() {
}

void DeviceProgrammer::program(const std::string& fname_bitfile) {
	// open bitfile and read data for programming
	BitFile bitfile;
	FILE* fh = fopen(fname_bitfile.c_str(), "rb");
	if (!fh)
		throw std::runtime_error("Could not open bitfile");
	if (bitfile.readFile(fh, STYLE_BIT) != 0)
		throw std::runtime_error("Error processing bitfile");

	// setup program algorithm and start programming
	ProgAlgXC3S progalg(m_jtag, FAMILY_XC6S);
	progalg.array_program(bitfile);
}
