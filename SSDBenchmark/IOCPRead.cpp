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
}

void IOCPRead::Release()
{
	CloseHandle(m_iocp);
	ReadAlgoBase::Release();
}

void IOCPRead::SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag)
{
	for (uint8_t i = 0; i < m_workThreadNumber; i++)
	{
		m_workThreadVecs.emplace_back(&IOCPRead::WorkThreadFunc, this, threadDataBag->m_endIndex - threadDataBag->m_startIndex);
	}

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
	for (uint8_t i = 0; i < m_workThreadNumber; i++)
	{
		if (m_workThreadVecs[i].joinable())
			m_workThreadVecs[i].join();
	}

	m_workThreadVecs.clear();

	for (auto& ioRequest : ioRequestVec)
	{
		ReleaseEvent(ioRequest.get());
	}
}

void IOCPRead::WorkThreadFunc(std::uint64_t ioRequestCnt)
{
	std::uint64_t completeCnt = 0;
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

		completeCnt++;

		if (completeCnt == ioRequestCnt) break;
	}
}


