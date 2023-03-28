#include <iostream>
#include <string>
#include <Windows.h>
#include <random>
#include <vector>
#include <thread>
#include <algorithm>
#include <numeric>
#include <iomanip>

#include "Config.h"
#include "ReadPerformanceTest.h"

int main()
{
	std::string fileName;
	int size, blockSize, numJobs;
	uint64_t testTime, batchSize;
	int readMethod;
	int ThreadNumberForSSDRead;
	bool isMemLock;
	uint64_t latencyCoutnerDuration;
	uint64_t qps;
	bool isPrintRawLatency;
	uint64_t iodepthThreshold;
	bool isETWTrace;
	uint64_t latencyThreshold;

	std::cout << "fileName: ";
	std::cin >> fileName;
	std::cout << "fileSize(GB): ";
	std::cin >> size;
	std::cout << "blockSize(KB): ";
	std::cin >> blockSize;
	std::cout << "threads number: ";
	std::cin >> numJobs;
	std::cout << "test time(s): ";
	std::cin >> testTime;
	std::cout << "batchSize: ";
	std::cin >> batchSize;
	std::cout << "readMethod(0-async 1-icop 2-iodepth 3-syncread): ";
	std::cin >> readMethod;
	std::cout << "ThreadNumberForSSDRead: ";
	std::cin >> ThreadNumberForSSDRead;
	std::cout << "memoryLock(0-off 1-on): ";
	std::cin >> isMemLock;
	std::cout << "latencyCoutnerDuration(s): ";
	std::cin >> latencyCoutnerDuration;
	std::cout << "dumpRawLatency(0-off 1-on): ";
	std::cin >> isPrintRawLatency;
	std::cout << "open ETW Trace(0-off 1-on): ";
	std::cin >> isETWTrace;
	
	if (isETWTrace)
	{
		std::cout << "latency threshold(us): ";
		std::cin >> latencyThreshold;
	}
	
	std::cout << "qps(per thread 0-not set): ";
	std::cin >> qps;

	if (readMethod == (int)Config::ReadMethod::IODepthRead)
	{
		std::cout << "iodpeth threshold: ";
		std::cin >> iodepthThreshold;
	}

	if (ThreadNumberForSSDRead > batchSize)
	{
		std::cout << "wrong threadNumberForSSDRead, smaller than batchsize" << std::endl;
		exit(-1);
	}

	Config::get().save(configName::TestFile, &fileName, configType::string);
	Config::get().save(configName::FileSize, &size, configType::uint8);
	Config::get().save(configName::RBlockSize, &blockSize, configType::uint8);
	Config::get().save(configName::RTime, &testTime, configType::uint64);
	Config::get().save(configName::ThreadsNum, &numJobs, configType::uint8);
	Config::get().save(configName::BatchSize, &batchSize, configType::uint64);
	Config::get().save(configName::ReadMethod, &readMethod, configType::uint8);
	Config::get().save(configName::ThreadNumberForSSDRead, &ThreadNumberForSSDRead, configType::uint8);
	Config::get().save(configName::MemoryLock, &isMemLock, configType::boolean);
	Config::get().save(configName::LatencyCoutnerDuration, &latencyCoutnerDuration, configType::uint64);
	Config::get().save(configName::QPS, &qps, configType::uint64);
	Config::get().save(configName::PrintRawLatency, &isPrintRawLatency, configType::boolean);
	Config::get().save(configName::IODepthThreshold, &iodepthThreshold, configType::uint64);
	Config::get().save(configName::OpenETWTracing, &isETWTrace, configType::boolean);
	Config::get().save(configName::LatencyThreshold, &latencyThreshold, configType::uint64);

	ReadPerformanceTest test;
	test.runReadBenchmark();

}