#pragma once

#include "ReadAlgoBase.h"

class SyncRead : public ReadAlgoBase
{
public:
	SyncRead() {};

private:
	virtual void SendMultipleIoRequest(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec, ThreadDataBag* threadDataBag) override;
	virtual void WaitForComplete(std::vector<std::unique_ptr<AsyncIORequest>>& ioRequestVec) override;
};


