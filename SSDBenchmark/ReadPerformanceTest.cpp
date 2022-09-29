#include "ReadPerformanceTest.h"
#include "AsyncRead.h"
#include "IOCPRead.h"

ReadPerformanceTest::ReadPerformanceTest()
{
	m_testTime = Config::get().getValUint64(configName::RTime) * 1000 * 1000;
	m_readMethod = (Config::ReadMethod)Config::get().getValUint8(configName::ReadMethod);
	m_threadNumber = Config::get().getValUint8(configName::ThreadsNum);
	m_fileName = Config::get().getValStr(configName::TestFile);
	m_fileSize = Config::get().getValUint8(configName::FileSize);
	m_memoryLock = Config::get().getValBoolean(configName::MemoryLock);
	m_bufferLenInBytes = (1 << 30);
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

void ReadPerformanceTest::start(std::uint8_t tId, TestStatisticBag& statisticBag) const
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
	default:
		break;
	}

	std::uint64_t elapsedTime = 0;
	while (elapsedTime < m_testTime)
	{
		auto startTime = std::chrono::high_resolution_clock::now();
		readAlgo->RunReadAlgo();
		auto endTime = std::chrono::high_resolution_clock::now();

		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();
		elapsedTime += duration;
	}

	double avgQueryLatency = 0, avgSendLatency = 0, avgWaitLatency = 0;
	size_t len = readAlgo->m_queryLatency.size();

	for (size_t i = 0; i < len; i++)
	{
		avgQueryLatency += (double)readAlgo->m_queryLatency[i] / len;
		avgSendLatency += (double)readAlgo->m_sendLatency[i] / len;
		avgWaitLatency += (double)readAlgo->m_waitLatency[i] / len;
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
	statisticBag.avgSendLatency = avgSendLatency;
	statisticBag.sendLatency50 = readAlgo->m_sendLatency[len * 0.5];
	statisticBag.sendLatency90 = readAlgo->m_sendLatency[len * 0.9];
	statisticBag.sendLatency95 = readAlgo->m_sendLatency[len * 0.95];
	statisticBag.sendLatency99 = readAlgo->m_sendLatency[len * 0.99];
	statisticBag.sendLatency999 = readAlgo->m_sendLatency[len * 0.999];
	statisticBag.avgWaitLatency = avgWaitLatency;
	statisticBag.waitLatency50 = readAlgo->m_waitLatency[len * 0.5];
	statisticBag.waitLatency90 = readAlgo->m_waitLatency[len * 0.9];
	statisticBag.waitLatency95 = readAlgo->m_waitLatency[len * 0.95];
	statisticBag.waitLatency99 = readAlgo->m_waitLatency[len * 0.99];
	statisticBag.waitLatency999 = readAlgo->m_waitLatency[len * 0.999];
	statisticBag.readTotalBytes = readAlgo->m_readTotalBytes;
	
	readAlgo->Release();
}

void ReadPerformanceTest::runReadBenchmark()
{
	if (m_memoryLock) SetPrivilege(SE_LOCK_MEMORY_NAME);
	prepareTestFile();

	std::vector<std::thread> threadVecs;
	std::vector<TestStatisticBag> latencyVecs(m_threadNumber);

	std::uint64_t readBandWidth = 0;

	for (std::uint8_t i = 0; i < m_threadNumber; i++)
	{
		threadVecs.emplace_back(&ReadPerformanceTest::start, this, i, std::ref(latencyVecs[i]));
	}

	for (auto& thread : threadVecs)
	{
		if (thread.joinable())
			thread.join();
	}

	for (std::uint8_t i = 0; i < m_threadNumber; i++)
	{
		std::cout << std::endl << std::endl << std::endl;
		std::cout << "thread " << (int)i << " latency: "<< std::endl;
		latencyVecs[i].print();
		readBandWidth += latencyVecs[i].readTotalBytes;
	}

	readBandWidth = (readBandWidth >> 20) / (m_testTime / 1000 / 1000);

	std::cout << "RBW(MB/s): " << readBandWidth << std::endl;
}