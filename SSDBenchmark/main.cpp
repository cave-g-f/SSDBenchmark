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

int main(int argc, char* argv[])
{
	std::string helpStr = "Usage: SSDBenchmark.exe filename size(GB) blockSize(KB) num_jobs testTime batchSize readMethod(0-async 1-icop) threadNumberForSSDRead memoryLock(0-off 1-on)";

	if (argc < 10)
	{
		std::cout << helpStr << std::endl;
		exit(-1);
	}

	std::string fileName(argv[1]);
	uint8_t size = atoi(argv[2]), blockSize = atoi(argv[3]), numJobs = atoi(argv[4]);
	uint64_t testTime = _atoi64(argv[5]), batchSize = _atoi64(argv[6]);
	uint8_t readMethod = atoi(argv[7]);
	uint8_t ThreadNumberForSSDRead = atoi(argv[8]);
	bool isMemLock = atoi(argv[9]);

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

	ReadPerformanceTest test;
	test.runReadBenchmark();

}