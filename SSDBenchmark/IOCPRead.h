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
	void WorkThreadFunc();

private:
	std::vector<std::thread> m_workThreadVecs;
	HANDLE m_iocp;
	uint8_t m_workThreadNumber;
	ULONG_PTR m_exitKey;
	HANDLE m_completeEvent;
};

