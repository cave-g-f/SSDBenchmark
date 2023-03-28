#include "ReadPerformanceTest.h"

#include <fstream>

ReadPerformanceTest::ReadPerformanceTest()
{
	m_testTime = Config::get().getValUint64(configName::RTime) * 1000 * 1000;
	m_readMethod = (Config::ReadMethod)Config::get().getValUint8(configName::ReadMethod);
	m_threadNumber = Config::get().getValUint8(configName::ThreadsNum);
	m_fileName = Config::get().getValStr(configName::TestFile);
	m_fileSize = Config::get().getValUint8(configName::FileSize);
	m_memoryLock = Config::get().getValBoolean(configName::MemoryLock);
	m_bufferLenInBytes = (1 << 30);
	m_latencyCounterDuration = Config::get().getValUint64(configName::LatencyCoutnerDuration);
	m_qps = Config::get().getValUint64(configName::QPS);
	m_printRawLatency = Config::get().getValBoolean(configName::PrintRawLatency);
	m_ioDepthThreshold = Config::get().getValUint64(configName::IODepthThreshold);

	if (m_qps)
	{
		m_timePerQuery = 1000 / m_qps;
		if (m_timePerQuery == 0)
		{
			std::cout << "qps per thread too large, cannot set, use default = 0" << std::endl;
			m_qps = 0;
		}
	}

	m_latencyFileHander.reserve(m_threadNumber);
	m_rawLatencyFileHander.reserve(m_threadNumber);
}

void ReadPerformanceTest::prepareTestFile()
{
	std::unique_ptr<uint8_t[]> writeBuffer = std::make_unique<uint8_t[]>(m_bufferLenInBytes + 512);

	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<uint16_t> distribute(0, MAXUINT8);

	HANDLE fileHandler = CreateFileA(m_fileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);
	if (fileHandler == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_EXISTS)
		{
			std::cout << "file already exists" << std::endl;
			return;
		}

		std::cout << "Create Destination Handle failed" << std::endl;
		exit(-1);
	}

	std::cout << "start write" << std::endl;

	for (uint8_t i = 0; i < m_fileSize; i++)
	{
		for (uint64_t j = 0; j < m_bufferLenInBytes; j++)
		{
			uint8_t number = distribute(gen);
			writeBuffer[i] = number;
		}

		std::uint32_t bytesWritten;
		auto writeSuccess = WriteFile(fileHandler, writeBuffer.get(), m_bufferLenInBytes, reinterpret_cast<LPDWORD>(&bytesWritten), NULL);

		if (!writeSuccess || bytesWritten == 0)
		{
			std::cout << "File write failed " << std::endl;
			CloseHandle(fileHandler);
			exit(-1);
		}
	}

	std::cout << "write successful" << std::endl;
	CloseHandle(fileHandler);
}

void ReadPerformanceTest::SetPrivilege(LPCWSTR privilegeName)
{
	std::cout << "setting privilege " << SE_LOCK_MEMORY_NAME << std::endl;
	HANDLE token;

	auto tokenRes = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token);

	if (!tokenRes)
	{
		std::cout << "get process token error, error code: " << GetLastError() << std::endl;
		exit(-1);
	}

	TOKEN_PRIVILEGES privileges;
	LUID luid;

	auto lookupRes = LookupPrivilegeValue(NULL, privilegeName, &luid);

	if (!lookupRes)
	{
		std::cout << "look up privilegevalue error, error code: " << GetLastError() << std::endl;
		exit(-1);
	}

	privileges.PrivilegeCount = 1;
	privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	privileges.Privileges[0].Luid = luid;

	auto adjustRes = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL);

	if (GetLastError() != ERROR_SUCCESS)
	{
		std::cout << "adjust privilegevalue error, error code: " << GetLastError() << std::endl;
		exit(-1);
	}
}

void ReadPerformanceTest::start(std::uint8_t tId, TestStatisticBag& statisticBag)
{
	std::unique_ptr<ReadAlgoBase> readAlgo;

	switch (m_readMethod)
	{
	case Config::ReadMethod::AsyncRead:
		readAlgo = std::make_unique<AsyncRead>();
		break;
	case Config::ReadMethod::IOCPRead:
		readAlgo = std::make_unique<IOCPRead>();
		break;
	case Config::ReadMethod::IODepthRead:
		readAlgo = std::make_unique<IODepthRead>(std::ref(m_ioDepth), m_ioDepthThreshold);
		break;
	case Config::ReadMethod::SyncRead:
		readAlgo = std::make_unique<SyncRead>();
		break;
	default:
		break;
	}

	std::uint64_t elapsedTime = 0;
	auto minuteStart = std::chrono::high_resolution_clock::now();
	while (elapsedTime < m_testTime)
	{
		auto queryStartTime = std::chrono::high_resolution_clock::now();
		readAlgo->RunReadAlgo();
		auto queryEndTime = std::chrono::high_resolution_clock::now();

		auto queryDuration = std::chrono::duration_cast<std::chrono::microseconds>(queryEndTime - queryStartTime).count();

		auto latencyDuration = std::chrono::duration_cast<std::chrono::microseconds>(queryEndTime - minuteStart).count();

		if (latencyDuration > m_latencyCounterDuration * 1000 * 1000)
		{
			setAndDumpLatencyFile(tId, readAlgo.get(), statisticBag);
			minuteStart = std::chrono::high_resolution_clock::now();
		}

		statisticBag.requestCnt += 1;

		if (m_qps)
		{
			auto durationInMilli = queryDuration / 1000;
			auto sleepTime = m_timePerQuery - durationInMilli;
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
		}

		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - queryStartTime).count();
		elapsedTime += duration;
	}

	statisticBag.readTotalBytes = readAlgo->m_readTotalBytes;
	
	readAlgo->Release();
}

void ReadPerformanceTest::setAndDumpLatencyFile(std::uint8_t tId, ReadAlgoBase* readAlgo, TestStatisticBag& statisticBag)
{
	double avgQueryLatency = 0, avgSendLatency = 0, avgWaitLatency = 0;
	size_t len = readAlgo->m_queryLatency.size();

	for (size_t i = 0; i < len; i++)
	{
		avgQueryLatency += (double)readAlgo->m_queryLatency[i] / len;
		avgSendLatency += (double)readAlgo->m_sendLatency[i] / len;
		avgWaitLatency += (double)readAlgo->m_waitLatency[i] / len;
	}

	std::cout << "start dump latency info..." << std::endl;

	if (m_printRawLatency)
	{
		for (int i = 0; i < len; i++)
		{
			m_rawLatencyFileHander[tId] << readAlgo->m_queryLatency[i] << "\n";
		}

		m_rawLatencyFileHander[tId] << "---- dump point ----\n";
	}

	std::sort(readAlgo->m_queryLatency.begin(), readAlgo->m_queryLatency.end());
	std::sort(readAlgo->m_sendLatency.begin(), readAlgo->m_sendLatency.end());
	std::sort(readAlgo->m_waitLatency.begin(), readAlgo->m_waitLatency.end());


	statisticBag.avgQueryLatency = avgQueryLatency;
	statisticBag.queryLatency50 = readAlgo->m_queryLatency[len * 0.5];
	statisticBag.queryLatency90 = readAlgo->m_queryLatency[len * 0.9];
	statisticBag.queryLatency95 = readAlgo->m_queryLatency[len * 0.95];
	statisticBag.queryLatency99 = readAlgo->m_queryLatency[len * 0.99];
	statisticBag.queryLatency999 = readAlgo->m_queryLatency[len * 0.999];
	statisticBag.queryLatencyMax = readAlgo->m_queryLatency[len - 1];
	statisticBag.avgSendLatency = avgSendLatency;
	statisticBag.sendLatency50 = readAlgo->m_sendLatency[len * 0.5];
	statisticBag.sendLatency90 = readAlgo->m_sendLatency[len * 0.9];
	statisticBag.sendLatency95 = readAlgo->m_sendLatency[len * 0.95];
	statisticBag.sendLatency99 = readAlgo->m_sendLatency[len * 0.99];
	statisticBag.sendLatency999 = readAlgo->m_sendLatency[len * 0.999];
	statisticBag.sendLatencyMax = readAlgo->m_sendLatency[len - 1];
	statisticBag.avgWaitLatency = avgWaitLatency;
	statisticBag.waitLatency50 = readAlgo->m_waitLatency[len * 0.5];
	statisticBag.waitLatency90 = readAlgo->m_waitLatency[len * 0.9];
	statisticBag.waitLatency95 = readAlgo->m_waitLatency[len * 0.95];
	statisticBag.waitLatency99 = readAlgo->m_waitLatency[len * 0.99];
	statisticBag.waitLatency999 = readAlgo->m_waitLatency[len * 0.999];
	statisticBag.waitLatencyMax = readAlgo->m_waitLatency[len - 1];

	readAlgo->clearLatency();


	std::string latencyStr = statisticBag.print();
	m_latencyFileHander[tId] << latencyStr;

	m_latencyFileHander[tId].flush();

	std::cout << "dump latency info end" << std::endl;
}

void ReadPerformanceTest::runReadBenchmark()
{
	if (m_memoryLock) SetPrivilege(SE_LOCK_MEMORY_NAME);
	prepareTestFile();

	std::vector<std::thread> threadVecs;
	std::vector<TestStatisticBag> latencyVecs(m_threadNumber);

	std::uint64_t readBandWidth = 0;
	std::uint64_t qps = 0;

	for (std::uint8_t i = 0; i < m_threadNumber; i++)
	{
		std::string fileName = "latency_" + std::to_string((int)i) + ".txt";
		auto fHandle = std::ofstream(fileName, std::ios_base::out);
		m_latencyFileHander.emplace_back(std::move(fHandle));

		if (m_printRawLatency)
		{
			std::string rawFileName = "rawlatency_" + std::to_string((int)i) + ".txt";
			auto rawFHandle = std::ofstream(rawFileName, std::ios_base::out);
			m_rawLatencyFileHander.emplace_back(std::move(rawFHandle));
		}

		threadVecs.emplace_back(&ReadPerformanceTest::start, this, i, std::ref(latencyVecs[i]));
	}

	for (auto& thread : threadVecs)
	{
		if (thread.joinable())
			thread.join();
	}

	for (std::uint8_t i = 0; i < m_threadNumber; i++)
	{
		readBandWidth += latencyVecs[i].readTotalBytes;
		qps += latencyVecs[i].requestCnt;
	}

	readBandWidth = (readBandWidth >> 20) / (m_testTime / 1000 / 1000);

	std::cout << "RBW(MB/s): " << readBandWidth << std::endl;

	std::cout << "QPS: " << qps / (m_testTime / 1000 / 1000) << std::endl;
}

ReadPerformanceTest::~ReadPerformanceTest()
{
	for (int i = 0; i < m_threadNumber; i++)
	{
		m_latencyFileHander[i].close();
	}
}
