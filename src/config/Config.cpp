#include "Config.h"

#include <iostream>
#include <fstream>
#include <string>
#include "json11.hpp"

Config Config::fromFile(std::string fname) {
	// read config file
	std::ifstream ifs("config.json");
	std::string content(
			(std::istreambuf_iterator<char>(ifs) ),
			(std::istreambuf_iterator<char>()    ));

	// parse json and store root object
	std::string err;
	json11::Json root(json11::Json::parse(content.c_str(), err));
    if (!err.empty()) throw std::runtime_error("Error reading configuration (" + err + ")");

	Config config;
	// build device descriptions from json structure
	// TODO: error checking
	for (auto& device_item: root["DeviceDescriptions"].array_items()) {
		DeviceManager::device_description_t desc;
		desc.name = device_item["name"].string_value();
		desc.serial_prefix = device_item["prefix"].string_value();
		desc.fname_bitfile = device_item["bitfile"].string_value();

		for (auto& v: device_item["watchlist"].array_items()) {
			Device::addr_port_t addr_port(v[0].int_value(), v[1].int_value());
			desc.watchlist.push_back(addr_port);
		}
		config.device_descriptions.push_back(std::move(desc));
	}

	config.port = root["Server"]["port"].int_value();

	return config;
}
