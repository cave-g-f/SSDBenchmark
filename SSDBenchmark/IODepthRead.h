#pragma once

#include "ReadAlgoBase.h"

#include <thread>

class IODepthRead : public ReadAlgoBase
{
public:
	IODepthRead(std::atomic<uint64_t>& ioDepth, std::uint64_t depthThreshold)
		:m_ioDepth(ioDepth),
		m_depthThreshold(depthThreshold)
	{
	};

private:
	virtual void SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag) override;
	virtual void WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec) override;

	std::vector<std::unique_ptr<AsyncIORequest>> m_pendingIOVecs;
	std::atomic<uint64_t>& m_ioDepth;
	std::uint64_t m_depthThreshold;
};

