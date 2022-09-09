#pragma once
#include <unordered_map>
#include <string>

class Config
{
public:
	enum class ConfigName
	{
		// GB
		WBufferLen,
		// KB
		RBlockSize,
		RTime,
		ThreadsNum,
		TestFile,
		// GB
		FileSize,
		ReadKeyNumberPerQuery,
		BatchSize,
		ReadMethod,
	};


	enum class ConfigType
	{
		uint8,
		string,
	};

private:
	using configTypeUint8 = std::unordered_map<ConfigName, uint8_t>;
	using configTypeStr = std::unordered_map<ConfigName, std::string>;

	configTypeUint8 configSetsUint8 =
	{
		{ConfigName::WBufferLen, 1},
		{ConfigName::ReadKeyNumberPerQuery, 1},
	};
	configTypeStr configSetsStr;

	Config() {};
	Config(const Config&) = delete;

public:
	static Config& get()
	{
		static Config instance;
		return instance;
	}
	
	uint8_t getValUint8(const ConfigName& key)
	{
		return configSetsUint8.at(key);
	}

	std::string getValStr(const ConfigName& key)
	{
		return configSetsStr.at(key);
	}
	
	void save(const ConfigName& key, void* value, const ConfigType& type)
	{
		if (type == ConfigType::uint8)
		{
			configSetsUint8[key] = *(reinterpret_cast<uint8_t*>(value));
		}
		else if (type == ConfigType::string)
		{
			configSetsStr[key] = *(reinterpret_cast<std::string*>(value));
		}
	}

};

using configName = Config::ConfigName;
using configType = Config::ConfigType;