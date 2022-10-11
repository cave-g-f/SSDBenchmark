#include "ReadAlgoBase.h"

#include <iostream>
#include <chrono>
#include <random>

ReadAlgoBase::ReadAlgoBase()
{
	m_readTotalBytes = 0;
	m_fileName = Config::get().getValStr(configName::TestFile);
	m_fileSize = Config::get().getValUint8(configName::FileSize);
	m_blockSize = Config::get().getValUint8(configName::RBlockSize);
	m_batchSize = Config::get().getValUint64(configName::BatchSize);
	m_blockNum = (static_cast<uint64_t>(m_fileSize) << 20) / m_blockSize;
	m_blockSizeInBytes = (DWORD)(m_blockSize) << 10;
	m_threadNumber = Config::get().getValUint8(configName::ThreadNumberForSSDRead);
	m_queryNumberForThread = m_batchSize / m_threadNumber;
	m_isMemoryLocked = false;
	m_memoryLock = Config::get().getValBoolean(configName::MemoryLock);

	m_overlaps.reserve(m_batchSize);
	m_overlaps.resize(m_batchSize);

	m_fileHandler = CreateFileA(m_fileName.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (m_fileHandler == INVALID_HANDLE_VALUE)
	{
		std::cout << "CreateFile Handle failed, error code: " << GetLastError() << std::endl;
		exit(-1);
	}
}

void ReadAlgoBase::RunReadAlgo()
{
	CreateOverlap();
	
	if (m_memoryLock && !m_isMemoryLocked) 
		LockMemory();

	Read();
	m_readTotalBytes += m_blockSizeInBytes * m_batchSize;
}

void ReadAlgoBase::ReleaseEvent(AsyncIORequest* ioRequest)
{
	ResetEvent(ioRequest->GetOverlap().hEvent);
	CloseHandle(ioRequest->GetOverlap().hEvent);
	ioRequest->GetOverlap().hEvent = nullptr;
}

void ReadAlgoBase::Release()
{
	CloseHandle(m_fileHandler);
}

void ReadAlgoBase::CreateOverlap()
{
	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<int64_t> distribute(0, m_blockNum - 1);

	for (uint64_t i = 0; i < m_batchSize; i++)
	{
		std::uint64_t startIndex = distribute(gen) * m_blockSizeInBytes;
		m_overlaps[i].Offset = startIndex & UINT32_MAX;
		m_overlaps[i].OffsetHigh = startIndex >> 32;
		m_overlaps[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);

		if (m_overlaps[i].hEvent == nullptr)
		{
			std::cout << "create event for overlap error, error code: " << GetLastError() << std::endl;
			exit(-1);
		}

		m_overlaps[i].Internal = 0;
		m_overlaps[i].InternalHigh = 0;
	}
}

void ReadAlgoBase::MultiRead(PTP_CALLBACK_INSTANCE, void* pContext, PTP_WORK)
{
	ThreadDataBag* threadDataBag = reinterpret_cast<ThreadDataBag*>(pContext);
	ReadAlgoBase* ReadAlgoBase = threadDataBag->m_readingTask;

	std::vector<std::unique_ptr<AsyncIORequest>> ioRequestVec;

	auto startTime = std::chrono::high_resolution_clock::now();

	ReadAlgoBase->SendMultipleIoRequest(ioRequestVec, threadDataBag);

	auto endSendTime = std::chrono::high_resolution_clock::now();

	ReadAlgoBase->WaitForComplete(ioRequestVec);

	auto endTime = std::chrono::high_resolution_clock::now();

	auto sendLatency = std::chrono::duration_cast<std::chrono::microseconds>(endSendTime - startTime).count();
	auto waitLatency = std::chrono::duration_cast<std::chrono::microseconds>(endTime - endSendTime).count();
	auto queryLatency = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime).count();

	threadDataBag->m_latencyInfo.m_queryLatency = queryLatency;
	threadDataBag->m_latencyInfo.m_sendLatency = sendLatency;
	threadDataBag->m_latencyInfo.m_waitLatency = waitLatency;
}

void ReadAlgoBase::Read()
{
	std::vector<ThreadDataBag> threadParameters;
	std::vector<PTP_WORK> multiWorks;

	threadParameters.reserve(m_threadNumber);
	multiWorks.reserve(m_threadNumber - 1);

	size_t startIndex = 0;
	for (uint8_t i = 0; i < m_threadNumber - 1; i++)
	{
		threadParameters.emplace_back(
			startIndex,
			startIndex + m_queryNumberForThread,
			&m_overlaps[0],
			this
		);

		PTP_WORK workItem = CreateThreadpoolWork(MultiRead, &threadParameters.back(), nullptr);

		if (workItem == nullptr)
		{
			std::cout << "Can't create workItem for multiThread, errorCode: " << GetLastError() << std::endl;
			threadParameters.pop_back();
			continue;
		}

		SubmitThreadpoolWork(workItem);
		multiWorks.push_back(workItem);
		startIndex += m_queryNumberForThread;
	}

	threadParameters.emplace_back(
		startIndex,
		m_batchSize,
		&m_overlaps[0],
		this
	);

	MultiRead(nullptr, &threadParameters.back(), nullptr);

	for (auto& workItem : multiWorks)
	{
		WaitForThreadpoolWorkCallbacks(workItem, false);
		CloseThreadpoolWork(workItem);
	}

	for (auto& threadData : threadParameters)
	{
		m_sendLatency.emplace_back(threadData.m_latencyInfo.m_sendLatency);
		m_queryLatency.emplace_back(threadData.m_latencyInfo.m_queryLatency);
		m_waitLatency.emplace_back(threadData.m_latencyInfo.m_waitLatency);
	}
}

void ReadAlgoBase::LockMemory()
{
	auto res = SetFileIoOverlappedRange(m_fileHandler, (PUCHAR)&m_overlaps[0], m_batchSize * sizeof(OVERLAPPED));

	if (!res)
	{
		std::cout << "setFileOverLapped error, error code: " << GetLastError() << std::endl;
	}
}
