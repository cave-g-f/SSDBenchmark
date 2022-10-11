#include "IOCPRead.h"

#include <iostream>

IOCPRead::IOCPRead()
{
	m_iocp = CreateIoCompletionPort(m_fileHandler, NULL, 0, 0);
	if (m_iocp == nullptr)
	{
		std::cout << "Create IO Completion Port Error, error code: " << GetLastError() << std::endl;
		exit(-1);
	}

	m_workThreadNumber = 1;

	if (m_threadNumber != 1)
	{
		std::cout << "IOCP not support multiple thread for SSD Read, please set ThreadNumberForSSDRead to 1" << std::endl;
		exit(-1);
	}

	m_completeEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (m_completeEvent == NULL)
	{
		std::cout << "m_completeEvent create error, error code: " << GetLastError() << std::endl;
		exit(-1);
	}

	m_exitKey = 1;

	for (uint8_t i = 0; i < m_workThreadNumber; i++)
	{
		m_workThreadVecs.emplace_back(&IOCPRead::WorkThreadFunc, this);
	}
}

void IOCPRead::Release()
{
	for (uint8_t i = 0; i < m_workThreadNumber; i++)
	{
		auto res = PostQueuedCompletionStatus(m_iocp, 0, m_exitKey, NULL);
		if (!res)
		{
			std::cout << "PostQueuedCompletionStatus error, error code: " << GetLastError() << std::endl;
			exit(-1);
		}
	}
	

	for (uint8_t i = 0; i < m_workThreadNumber; i++)
	{
		if (m_workThreadVecs[i].joinable())
			m_workThreadVecs[i].join();
	}

	m_workThreadVecs.clear();

	CloseHandle(m_iocp);
	CloseHandle(m_completeEvent);
	ReadAlgoBase::Release();
}

void IOCPRead::SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag)
{
	for (uint64_t i = threadDataBag->m_startIndex; i < threadDataBag->m_endIndex; i++)
	{
		std::unique_ptr<AsyncIORequest> asyncRequest = std::make_unique<AsyncIORequest>(m_blockSizeInBytes, threadDataBag->m_overlappedInfos[i]);
		ReadFile(m_fileHandler, asyncRequest->GetBuffer(), m_blockSizeInBytes, &(asyncRequest->m_returnBytes), &asyncRequest->GetOverlap());

		asyncRequest->m_errorCode = GetLastError();
		if (!asyncRequest->IsReadSucceeded())
		{
			std::cout << "ReadFile error, error code: " << asyncRequest->m_errorCode << std::endl;
			exit(-1);
		}
		else
		{
			ioRequestVec.emplace_back(std::move(asyncRequest));
		}
	}
}

void IOCPRead::WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec)
{
	auto res = WaitForSingleObject(m_completeEvent, INFINITE);
	if (res != WAIT_OBJECT_0)
	{
		std::cout << "wait for complete error, wair status: " << res << std::endl;
		exit(-1);
	}
}

void IOCPRead::WorkThreadFunc()
{
	uint32_t completIO = 0;
	SetThreadAffinityMask(GetCurrentThread(), 1);
	SetThreadPriority(GetCurrentThread(), 2);
	while (1)
	{
		LPOVERLAPPED overlap;
		ULONG_PTR completionKey = 0;
		DWORD readBytes = 0;

		auto res = GetQueuedCompletionStatus(m_iocp, &readBytes, &completionKey, &overlap, INFINITE);

		if (!res)
		{
			std::cout << "GetQueuedCompletionStatus error , error code: " << GetLastError() << std::endl;
			exit(-1);
		}

		completIO++;

		if (completIO == m_batchSize)
		{
			SetEvent(m_completeEvent);
			completIO = 0;
		}

		if (completionKey == m_exitKey)
		{
			break;
		}
	}
}


