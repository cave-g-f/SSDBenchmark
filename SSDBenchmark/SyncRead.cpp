#include "SyncRead.h"
#include "AsyncIO.h"

#include <iostream>

void SyncRead::WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec)
{
}

void SyncRead::SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag)
{
	for (uint64_t i = threadDataBag->m_startIndex; i < threadDataBag->m_endIndex; i++)
	{
		std::unique_ptr<AsyncIORequest> asyncRequest = std::make_unique<AsyncIORequest>(m_blockSizeInBytes, threadDataBag->m_overlappedInfos[i]);
		LONG highOffset = asyncRequest->GetOverlap().OffsetHigh;
		SetFilePointer(m_fileHandler, asyncRequest->GetOverlap().Offset, &highOffset, FILE_BEGIN);
		auto res = ReadFile(m_fileHandler, asyncRequest->GetBuffer(), m_blockSizeInBytes, &(asyncRequest->m_returnBytes), NULL);

		asyncRequest->m_errorCode = GetLastError();

		if (!asyncRequest->IsReadSucceeded())
		{
			std::cout << "ReadFile error, error code: " << asyncRequest->m_errorCode << std::endl;
			ReleaseEvent(asyncRequest.get());
		}

		ioRequestVec.emplace_back(std::move(asyncRequest));
	}
}
