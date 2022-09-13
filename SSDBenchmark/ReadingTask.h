#pragma once
#include <Windows.h>
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <thread>

#include "Config.h"

class ReadingTask
{
public:

	enum class ReadMethod
	{
		AsyncRead = 0,
		AsyncLockRead,
		IOCPRead,
		IOCPLockRead,
	};

	ReadingTask(
		std::uint32_t readKeyNumberPerQuery,
		std::uint8_t testTime,
		std::uint8_t tId);

	void Run();

	std::uint64_t GetElapsedTime()
	{
		return m_elapsedTime;
	}

private:
	void IOCPRead();
	void IOCPLockRead();
	void AsyncLockRead();
	void AsyncRead();

public:
	std::vector<std::uint64_t> m_queryLatency;
	std::vector<std::uint64_t> m_sendLatency;
	std::vector<std::uint64_t> m_waitLatency;
	std::uint64_t m_readTotalBytes;

	std::uint8_t m_tId;
	std::uint64_t m_testTime;
	std::uint32_t m_readKeyNumberPerQuery;
	std::string m_fileName;
	std::uint64_t m_elapsedTime;
	std::uint8_t m_fileSize;
	std::uint8_t m_blockSize;
	std::uint8_t m_batchSize;
	std::uint64_t m_blockNum;
	ReadMethod m_readMethod;
	DWORD m_blockSizeInBytes;
};