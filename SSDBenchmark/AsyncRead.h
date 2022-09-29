#pragma once

#include "ReadAlgoBase.h"

class AsyncRead : public ReadAlgoBase
{
public:
	AsyncRead() {};

private:
	virtual void SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag) override;
	virtual void WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec) override;
};

