#pragma once

#include <vector>
#include <Windows.h>

#include "Config.h"
#include "AsyncIO.h"


class ReadAlgoBase
{
public:
	struct LatencyInfoBag
	{
		std::uint64_t m_sendLatency;
		std::uint64_t m_waitLatency;
		std::uint64_t m_queryLatency;
	};

	struct ThreadDataBag
	{
		ThreadDataBag(
			uint64_t startIndex,
			uint64_t endIndex,
			OVERLAPPED* overlappedInfos,
			ReadAlgoBase* readingTask) :
			m_startIndex(startIndex),
			m_endIndex(endIndex),
			m_overlappedInfos(overlappedInfos),
			m_readingTask(readingTask),
			m_handlesInfos(nullptr)
		{}

		uint64_t m_startIndex;
		uint64_t m_endIndex;
		HANDLE* m_handlesInfos;
		OVERLAPPED* m_overlappedInfos;
		ReadAlgoBase* m_readingTask;
		LatencyInfoBag m_latencyInfo;
	};

public:
	ReadAlgoBase();
	void RunReadAlgo();
	virtual void Release();

protected:
	void Read();
	void LockMemory();
	void ReleaseEvent(AsyncIORequest* ioRequest);
	void CreateOverlap();
	virtual void SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag) = 0;
	static void CALLBACK MultiRead(PTP_CALLBACK_INSTANCE, void* pContext, PTP_WORK);
	virtual void WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec) = 0;

public:
	std::vector<std::uint64_t> m_queryLatency;
	std::vector<std::uint64_t> m_sendLatency;
	std::vector<std::uint64_t> m_waitLatency;
	std::uint64_t m_readTotalBytes;

protected:
	std::vector<OVERLAPPED> m_overlaps;

	HANDLE m_fileHandler;
	std::uint8_t m_tId;
	std::string m_fileName;
	std::uint8_t m_fileSize;
	std::uint8_t m_blockSize;
	std::uint64_t m_batchSize;
	std::uint64_t m_blockNum;
	std::uint64_t m_threadNumber;
	std::uint64_t m_queryNumberForThread;
	bool m_memoryLock;
	bool m_isMemoryLocked;
	DWORD m_blockSizeInBytes;
};
