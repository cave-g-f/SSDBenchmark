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
		BatchSize,
		ReadMethod,
		ThreadNumberForSSDRead,
		MemoryLock,
		LatencyCoutnerDuration,
		QPS,
		PrintRawLatency,
		IODepthThreshold,
		OpenETWTracing,
		LatencyThreshold,
	};

	enum class ReadMethod
	{
		AsyncRead = 0,
		IOCPRead,
		IODepthRead,
		SyncRead,
	};


	enum class ConfigType
	{
		uint8,
		uint64,
		string,
		boolean,
	};

private:
	using configTypeUint8 = std::unordered_map<ConfigName, uint8_t>;
	using configTypeUint64 = std::unordered_map<ConfigName, uint64_t>;
	using configTypeStr = std::unordered_map<ConfigName, std::string>;
	using configTypeBoolean = std::unordered_map<ConfigName, bool>;

	configTypeUint8 configSetsUint8 =
	{
		{ConfigName::WBufferLen, 1},
	};
	configTypeStr configSetsStr;
	configTypeUint64 configSetsUint64;
	configTypeBoolean configSetsBoolean;

	Config() {};
	Config(const Config&) = delete;

public:
	static Config& get()
	{
		static Config instance;
		return instance;
	}
	
	std::uint8_t getValUint8(const ConfigName& key)
	{
		return configSetsUint8.at(key);
	}

	std::string getValStr(const ConfigName& key)
	{
		return configSetsStr.at(key);
	}

	std::uint64_t getValUint64(const ConfigName& key)
	{
		return configSetsUint64.at(key);
	}

	bool getValBoolean(const ConfigName& key)
	{
		return configSetsBoolean.at(key);
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
		else if (type == ConfigType::uint64)
		{
			configSetsUint64[key] = *(reinterpret_cast<uint64_t*>(value));
		}
		else if (type == ConfigType::boolean)
		{
			configSetsBoolean[key] = *(reinterpret_cast<bool*>(value));
		}
	}

};

using configName = Config::ConfigName;
using configType = Config::ConfigType;