#include "DeviceProgrammer.h"
#include "xc3sprog/bitfile.h"
#include "xc3sprog/progalgxc3s.h"
#include <stdio.h>
#include <string.h>
#include <iostream>
#include <stdexcept>

const int XC6_XC7_IRLEN = 6;

DeviceProgrammer::DeviceProgrammer(libusb_device* dev) : m_ioftdi(dev, INTERFACE_B), m_jtag(&m_ioftdi) {
	// check jtag chain
	int n = m_jtag.getChain();
	if (n != 1)
		throw std::runtime_error("Error in Jtag chain");
        auto idcode = m_jtag.getDeviceID(0);

	if (idcode == 0x24001093) {
            // Spartan6 LX9
            m_family = FAMILY_XC6S;
        } else if (idcode == 0x0362d093) {
            // Artix9 35T
            m_family = FAMILY_XC7;
        } else
            throw std::runtime_error("Unexpected Jtag device ID");

	// prepare jtag interface
	m_jtag.setDeviceIRLength(0, XC6_XC7_IRLEN);
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
	ProgAlgXC3S progalg(m_jtag, m_family);
	progalg.array_program(bitfile);
}
