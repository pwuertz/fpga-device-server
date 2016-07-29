//-----------------------------------------------------------------------------
// Author: Peter Würtz, TU Kaiserslautern (2016)
//
// Distributed under the terms of the GNU General Public License Version 3.
// The full license is in the file COPYING.txt, distributed with this software.
//-----------------------------------------------------------------------------

#ifndef CONFIG_CONFIG_H_
#define CONFIG_CONFIG_H_

#include "../devices/DeviceManager.h"

struct Config {
	DeviceManager::device_descriptions_t device_descriptions;
	int port;

	static Config fromFile(std::string fname);
};

#endif /* CONFIG_CONFIG_H_ */
