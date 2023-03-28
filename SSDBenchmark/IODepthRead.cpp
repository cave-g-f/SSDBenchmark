#include "IODepthRead.h"
#include "AsyncIO.h"

#include <iostream>

void IODepthRead::WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec)
{
	std::vector<HANDLE> handlersList;
	handlersList.reserve(m_pendingIOVecs.size());

	for (auto& ioRequest : m_pendingIOVecs)
	{
		handlersList.emplace_back(ioRequest->GetOverlap().hEvent); 
		ioRequestVec.emplace_back(std::move(ioRequest));
	}

	for (uint64_t i = 0; i < m_pendingIOVecs.size(); i += MAXIMUM_WAIT_OBJECTS)
	{
		DWORD remainRequest = m_pendingIOVecs.size() - i;
		DWORD waitCnt = remainRequest > MAXIMUM_WAIT_OBJECTS ? MAXIMUM_WAIT_OBJECTS : remainRequest;
		DWORD waitRes = WaitForMultipleObjects(waitCnt, &handlersList[i], TRUE, INFINITE);

		if (waitRes == WAIT_FAILED)
		{
			std::cout << "wait failed error code: " << GetLastError() << std::endl;
			continue;
		}
	}

	int size = m_pendingIOVecs.size();
	m_ioDepth -= size;

	m_pendingIOVecs.clear();
}

void IODepthRead::SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag)
{
	for (int i = threadDataBag->m_startIndex; i < threadDataBag->m_endIndex; i++)
	{
		std::unique_ptr<AsyncIORequest> asyncRequest = std::make_unique<AsyncIORequest>(m_blockSizeInBytes, threadDataBag->m_overlappedInfos[i]);

		if (m_ioDepth > 100)
			printf("iodepth: %d\n", m_ioDepth.load());

		if (m_ioDepth < m_depthThreshold)
		{
			auto res = ReadFile(m_fileHandler, asyncRequest->GetBuffer(), m_blockSizeInBytes, &(asyncRequest->m_returnBytes), &asyncRequest->GetOverlap());

			asyncRequest->m_errorCode = GetLastError();

			if (!asyncRequest->IsReadSucceeded())
			{
				std::cout << "ReadFile error, error code: " << asyncRequest->m_errorCode << std::endl;
				ReleaseEvent(asyncRequest.get());
			}

			m_pendingIOVecs.emplace_back(std::move(asyncRequest));
			m_ioDepth++;
		}
		else
		{
			WaitForComplete(ioRequestVec);
			i = i - 1;
		}
	}
}
