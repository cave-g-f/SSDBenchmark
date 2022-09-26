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
#include "ReadingTask.h"

void prepareTestFile()
{
	const std::string& fileName = Config::get().getValStr(configName::TestFile);
	const uint8_t& size = Config::get().getValUint8(configName::FileSize);
	const uint8_t& bufferLenGB = Config::get().getValUint8(configName::WBufferLen);

	uint64_t bufferLenB = (static_cast<uint64_t>(bufferLenGB) << 30);

	std::unique_ptr<uint8_t[]> writeBuffer = std::make_unique<uint8_t[]>(bufferLenB + 512);

	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<uint16_t> distribute(0, MAXUINT8);

	HANDLE fileHandler = CreateFileA(fileName.c_str(), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, FILE_FLAG_NO_BUFFERING, NULL);
	if (fileHandler == INVALID_HANDLE_VALUE)
	{
		if (GetLastError() == ERROR_FILE_EXISTS)
		{
			std::cout << "file already exists" << std::endl;
			return;
		}

		std::cout << "Create Destination Handle failed" << std::endl;
		CloseHandle(fileHandler);
		exit(-1);
	}

	std::cout << "start write" << std::endl;

	for (uint8_t i = 0; i < size; i++)
	{
		for (uint64_t j = 0; j < bufferLenB; j++)
		{
			uint8_t number = distribute(gen);
			writeBuffer[i] = number;
		}

		std::uint32_t bytesWritten;
		auto writeSuccess = WriteFile(fileHandler, writeBuffer.get(), bufferLenB, reinterpret_cast<LPDWORD>(&bytesWritten), NULL);

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

void runReadBenchmark()
{
	const uint8_t& threadNum = Config::get().getValUint8(configName::ThreadsNum);
	const std::string& fileName = Config::get().getValStr(configName::TestFile);
	const uint8_t& size = Config::get().getValUint8(configName::FileSize);
	const uint8_t& bufferLenGB = Config::get().getValUint8(configName::WBufferLen);
	const uint64_t& testTime = Config::get().getValUint64(configName::RTime);

	std::vector<std::thread> threadVecs;
	std::vector<ReadingTask> tasks;

	for (uint8_t i = 0; i < threadNum; i++)
	{
		tasks.emplace_back(1, 0);
	}

	for (uint8_t i = 0; i < threadNum; i++)
	{
		threadVecs.emplace_back(&ReadingTask::Run, &tasks[i]);
	}

	for (auto& thread : threadVecs)
	{
		if (thread.joinable())
			thread.join();
	}

	std::vector<uint64_t> totalLatency;
	std::vector<uint64_t> totalSendLatency;
	std::vector<uint64_t> totalWaitLatency;
	std::uint64_t totalReadBytes = 0;

	for (auto& task : tasks)
	{
		totalLatency.insert(totalLatency.end(), task.m_queryLatency.begin(), task.m_queryLatency.end());
		totalSendLatency.insert(totalSendLatency.end(), task.m_sendLatency.begin(), task.m_sendLatency.end());
		totalWaitLatency.insert(totalWaitLatency.end(), task.m_waitLatency.begin(), task.m_waitLatency.end());
		totalReadBytes += task.m_readTotalBytes;
	}

	size_t len = totalLatency.size();
	double avgLatency = 0, avgSendLatency = 0, avgWaitLatency = 0;

	for (size_t i = 0; i < len; i++)
	{
		avgLatency += (double)totalLatency[i] / len;
		avgSendLatency += (double)totalSendLatency[i] / len;
		avgWaitLatency += (double)totalWaitLatency[i] / len;
	}

	std::sort(totalLatency.begin(), totalLatency.end());
	std::sort(totalSendLatency.begin(), totalSendLatency.end());
	std::sort(totalWaitLatency.begin(), totalWaitLatency.end());

	std::cout << "-----total latency-----" << std::endl;
	std::cout << "avg: " << avgLatency << std::endl;
	std::cout << "50th: " << totalLatency[len / 2] << std::endl;
	std::cout << "90th: " << totalLatency[len * 0.9] << std::endl;
	std::cout << "95th: " << totalLatency[len * 0.95] << std::endl;
	std::cout << "99th: " << totalLatency[len * 0.99] << std::endl;
	std::cout << "99.9th: " << totalLatency[len * 0.999] << std::endl;
	std::cout << "-----send latency------" << std::endl;
	std::cout << "avg: " << avgSendLatency << std::endl;
	std::cout << "50th: " << totalSendLatency[len / 2] << std::endl;
	std::cout << "90th: " << totalSendLatency[len * 0.9] << std::endl;
	std::cout << "95th: " << totalSendLatency[len * 0.95] << std::endl;
	std::cout << "99th: " << totalSendLatency[len * 0.99] << std::endl;
	std::cout << "99.9th: " << totalSendLatency[len * 0.999] << std::endl;
	std::cout << "-----wait latency------" << std::endl;
	std::cout << "avg: " << avgWaitLatency << std::endl;
	std::cout << "50th: " << totalWaitLatency[len / 2] << std::endl;
	std::cout << "90th: " << totalWaitLatency[len * 0.9] << std::endl;
	std::cout << "95th: " << totalWaitLatency[len * 0.95] << std::endl;
	std::cout << "99th: " << totalWaitLatency[len * 0.99] << std::endl;
	std::cout << "99.9th: " << totalWaitLatency[len * 0.999] << std::endl;

	std::cout << "RBW: " << (totalReadBytes >> 20) / testTime << std::endl;
}

void SetPrivilege(LPCWSTR privilegeName)
{
	std::cout << "setting privilege " << SE_LOCK_MEMORY_NAME << std::endl;
	HANDLE token;

	auto tokenRes = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &token);

	if (!tokenRes)
	{
		std::cout << "get process token error" << std::endl;
		std::cout << GetLastError() << std::endl;
		exit(-1);
	}

	TOKEN_PRIVILEGES privileges;
	LUID luid;

	auto lookupRes = LookupPrivilegeValue(NULL, privilegeName, &luid);

	if (!lookupRes)
	{
		std::cout << "look up privilegevalue error" << std::endl;
		std::cout << GetLastError() << std::endl;
		exit(-1);
	}

	privileges.PrivilegeCount = 1;
	privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	privileges.Privileges[0].Luid = luid;

	auto adjustRes = AdjustTokenPrivileges(token, FALSE, &privileges, sizeof(TOKEN_PRIVILEGES), NULL, NULL);

	if (GetLastError() != ERROR_SUCCESS)
	{
		std::cout << "adjust privilegevalue error" << std::endl;
		std::cout << GetLastError() << std::endl;
		exit(-1);
	}
}

int main(int argc, char* argv[])
{
	std::string helpStr = "Usage: SSDBenchmark.exe filename size(GB) blockSize(KB) num_jobs testTime batchSize readMethod(0-async 1-asyncLock 2-icop 3-icopLock) readSpeed threadNumberForSSDRead";

	if (argc < 10)
	{
		std::cout << helpStr << std::endl;
		exit(-1);
	}

	std::string fileName(argv[1]);
	uint8_t size = atoi(argv[2]), blockSize = atoi(argv[3]), numJobs = atoi(argv[4]);
	uint64_t testTime = _atoi64(argv[5]), batchSize = _atoi64(argv[6]);
	uint8_t readMethod = atoi(argv[7]);
	uint64_t readSpeed = _atoi64(argv[8]);
	uint8_t ThreadNumberForSSDRead = atoi(argv[9]);

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
	Config::get().save(configName::ReadSpeed, &readSpeed, configType::uint64);
	Config::get().save(configName::ThreadNumberForSSDRead, &ThreadNumberForSSDRead, configType::uint8);
	std::cout << "readMethod: " << (int)readMethod << std::endl;
	if (readMethod == (uint8_t)ReadingTask::ReadMethod::AsyncLockRead ||
		readMethod == (uint8_t)ReadingTask::ReadMethod::IOCPLockRead)
		SetPrivilege(SE_LOCK_MEMORY_NAME);

	prepareTestFile();
	runReadBenchmark();

}