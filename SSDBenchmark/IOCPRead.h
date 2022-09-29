#pragma once

#include "ReadAlgoBase.h"

#include <thread>

class IOCPRead : public ReadAlgoBase
{
public:
	IOCPRead();
	virtual void Release() override;

private:
	virtual void SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag) override;
	virtual void WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec) override;
	void WorkThreadFunc(std::uint64_t ioRequestCnt);

private:
	std::vector<std::thread> m_workThreadVecs;
	HANDLE m_iocp;
	uint8_t m_workThreadNumber;
};

