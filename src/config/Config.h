#ifndef CONFIG_CONFIG_H_
#define CONFIG_CONFIG_H_

#include "../devices/DeviceManager.h"

struct Config {
	DeviceManager::device_descriptions_t device_descriptions;
	int port;

	static Config fromFile(std::string fname);
};

#endif /* CONFIG_CONFIG_H_ */
