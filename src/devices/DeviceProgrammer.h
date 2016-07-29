//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#ifndef SRC_DEVICES_DEVICEPROGRAMMER_H_
#define SRC_DEVICES_DEVICEPROGRAMMER_H_

#include <libusb.h>
#include "xc3sprog/ioftdi.h"
#include "xc3sprog/jtag.h"
#include <string>

class DeviceProgrammer {
public:
	DeviceProgrammer(libusb_device* dev);
	virtual ~DeviceProgrammer();

	void program(const std::string& fname_bitfile);

private:
	IOFtdi m_ioftdi;
	Jtag m_jtag;
        int m_family;
};

#endif /* SRC_DEVICES_DEVICEPROGRAMMER_H_ */
