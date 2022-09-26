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

	ReadingTask(std::uint32_t readKeyNumberPerQuery,
		std::uint8_t tId);
	~ReadingTask() = default;

	void Run();

	std::uint64_t GetElapsedTime()
	{
		return m_elapsedTime;
	}

private:

	struct ThreadDataBag
	{
		ThreadDataBag(uint64_t startIndex,
			uint64_t endIndex,
			HANDLE* handlesInfos,
			OVERLAPPED* overlappedInfos,
			ReadingTask* readingTask) :
			m_startIndex(startIndex),
			m_endIndex(endIndex),
			m_handlesInfos(handlesInfos),
			m_overlappedInfos(overlappedInfos),
			m_readingTask(readingTask)
		{}

		uint64_t m_startIndex;
		uint64_t m_endIndex;
		HANDLE* m_handlesInfos;
		OVERLAPPED* m_overlappedInfos;
		ReadingTask* m_readingTask;
	};

	void IOCPRead();
	void IOCPLockRead();
	void AsyncLockRead();
	void AsyncRead();

	static void CALLBACK MultiRead(PTP_CALLBACK_INSTANCE, void* pContext, PTP_WORK);

	HANDLE m_file;
	std::vector<std::shared_ptr<uint8_t[]>> m_buffer;

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
	std::uint64_t m_batchSize;
	std::uint64_t m_queryTime;
	std::uint64_t m_blockNum;
	std::uint64_t m_readSpeed;
	std::uint64_t m_threadNumber;
	ReadMethod m_readMethod;
	DWORD m_blockSizeInBytes;
};