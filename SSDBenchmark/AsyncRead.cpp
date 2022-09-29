#include "AsyncRead.h"
#include "AsyncIO.h"

#include <iostream>

void AsyncRead::WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec)
{
	std::vector<HANDLE> handlersList;
	handlersList.reserve(ioRequestVec.size());

	for (auto& ioRequest : ioRequestVec)
	{
		handlersList.emplace_back(ioRequest->GetOverlap().hEvent);
	}

	for (uint64_t i = 0; i < ioRequestVec.size(); i += MAXIMUM_WAIT_OBJECTS)
	{
		DWORD remainRequest = ioRequestVec.size() - i;
		DWORD waitCnt = remainRequest > MAXIMUM_WAIT_OBJECTS ? MAXIMUM_WAIT_OBJECTS : remainRequest;
		DWORD waitRes = WaitForMultipleObjects(waitCnt, &handlersList[i], TRUE, INFINITE);

		if (waitRes == WAIT_FAILED)
		{
			std::cout << "wait failed error code: " << GetLastError() <<  std::endl;
			continue;
		}
	}

	for (auto& ioRequest : ioRequestVec)
	{
		ReleaseEvent(ioRequest.get());
	}
}

void AsyncRead::SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag)
{
	for (uint64_t i = threadDataBag->m_startIndex; i < threadDataBag->m_endIndex; i++)
	{
		std::unique_ptr<AsyncIORequest> asyncRequest = std::make_unique<AsyncIORequest>(m_blockSizeInBytes, threadDataBag->m_overlappedInfos[i]);
		auto res = ReadFile(m_fileHandler, asyncRequest->GetBuffer(), m_blockSizeInBytes, &(asyncRequest->m_returnBytes), &asyncRequest->GetOverlap());

		asyncRequest->m_errorCode = GetLastError();

		if (!asyncRequest->IsReadSucceeded())
		{
			std::cout << "ReadFile error, error code: " << asyncRequest->m_errorCode << std::endl;
			ReleaseEvent(asyncRequest.get());
		}
		else
		{
			ioRequestVec.emplace_back(std::move(asyncRequest));
		}
	}
}
