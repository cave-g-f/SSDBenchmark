#include "ReadingTask.h"

ReadingTask::ReadingTask(
	std::uint32_t readKeyNumberPerQuery,
	std::uint8_t testTime,
	std::uint8_t tId)
	: m_readKeyNumberPerQuery(readKeyNumberPerQuery)
	, m_testTime(testTime)
	, m_tId(tId)
{
	m_elapsedTime = 0;
	m_readTotalBytes = 0;
	m_fileName = Config::get().getValStr(configName::TestFile);
	m_fileSize = Config::get().getValUint8(configName::FileSize);
	m_testTime = (std::uint64_t)(Config::get().getValUint8(configName::RTime)) * 1000 * 1000;
	m_blockSize = Config::get().getValUint8(configName::RBlockSize);
	m_batchSize = Config::get().getValUint8(configName::BatchSize);
	m_blockNum = (static_cast<uint64_t>(m_fileSize) << 20) / m_blockSize;
	m_blockSizeInBytes = (DWORD)(m_blockSize) << 10;
	m_readMethod = (ReadMethod)Config::get().getValUint8(configName::ReadMethod);
}

void ReadingTask::IOCPLockRead()
{
	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<int64_t> distribute(0, m_blockNum - 1);

	HANDLE fileHandler = CreateFileA(m_fileName.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (fileHandler == INVALID_HANDLE_VALUE)
	{
		std::cout << "Create Source Handle failed" << std::endl;
		std::cout << GetLastError() << std::endl;
		CloseHandle(fileHandler);
		exit(-1);
	}

	HANDLE iocp = CreateIoCompletionPort(fileHandler, NULL, 0, 0);

	ULONG_PTR threadExitKey = 1;
	uint8_t workThreadNum = 4;
	std::vector<OVERLAPPED_ENTRY> overlapEntrys(m_batchSize);
	std::vector<OVERLAPPED> overlaps(m_batchSize);
	std::vector<std::unique_ptr<uint8_t[]>> buffers;
	std::vector<std::thread> workThreads;
	std::vector<std::uint64_t> completeCnt(workThreadNum, 0);

	for (uint8_t i = 0; i < m_batchSize; i++)
	{
		buffers.emplace_back(std::make_unique<uint8_t[]>(m_blockSizeInBytes));
	}

	auto WorkThreadFunc = [iocp, this, threadExitKey, &completeCnt](uint8_t tid) {
		int cnt = 0;
		while (1)
		{
			LPOVERLAPPED overlap;
			ULONG_PTR completionKey = 0;
			DWORD readBytes = 0;

			auto res = GetQueuedCompletionStatus(iocp, &readBytes, &completionKey, &overlap, INFINITE);

			if (completionKey == threadExitKey)
			{
				return 0;
			}

			if (!res)
			{
				std::cout << GetLastError() << std::endl;
				std::cout << "GetQueuedCompletionStatus error " << std::endl;
				exit(-1);
			}

			if (readBytes == this->m_blockSizeInBytes)
			{
				completeCnt[tid] += 1;
			}
		}
	};

	auto setRes = SetFileIoOverlappedRange(fileHandler, (PUCHAR)&overlaps[0], m_batchSize * sizeof(OVERLAPPED));

	if (!setRes)
	{
		std::cout << GetLastError() << std::endl;
		std::cout << "setFileOverLapped error" << std::endl;
		exit(-1);
	}

	uint64_t totalCnt = 0;
	while (m_elapsedTime < m_testTime)
	{
		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			std::uint64_t startIndex = distribute(gen) * m_blockSizeInBytes;
			overlaps[i].Offset = startIndex & UINT32_MAX;
			overlaps[i].OffsetHigh = startIndex >> 32;
			overlaps[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			overlaps[i].Internal = 0;
			overlaps[i].InternalHigh = 0;
			overlapEntrys[i].dwNumberOfBytesTransferred = m_batchSize;
			overlapEntrys[i].lpOverlapped = &overlaps[i];
			overlapEntrys[i].lpCompletionKey = 0;
			overlapEntrys[i].Internal = 0;
		}

		for (uint8_t i = 0; i < workThreadNum; i++)
		{
			workThreads.emplace_back(WorkThreadFunc, i);
		}

		auto startTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			auto res = ReadFile(fileHandler, buffers[i].get(), m_blockSizeInBytes, NULL, &overlaps[i]);

			if (!res && GetLastError() != ERROR_IO_PENDING)
			{
				std::cout << "File read failed " << std::endl;
				std::cout << GetLastError() << std::endl;
				CloseHandle(fileHandler);
				exit(-1);
			}
		}

		auto endIOSendTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < workThreadNum; i++)
		{
			OVERLAPPED overlap;
			PostQueuedCompletionStatus(iocp, 0, threadExitKey, &overlap);
		}

		for (uint8_t i = 0; i < workThreadNum; i++)
		{
			if (workThreads[i].joinable())
				workThreads[i].join();
		}
		workThreads.clear();

		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
		auto sendDuration = std::chrono::duration_cast<std::chrono::microseconds>(endIOSendTime - startTime);
		auto waitDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - endIOSendTime);
		m_queryLatency.emplace_back(duration.count());
		m_sendLatency.emplace_back(sendDuration.count());
		m_waitLatency.emplace_back(waitDuration.count());
		m_elapsedTime += duration.count();
		m_readTotalBytes += m_blockSizeInBytes * m_batchSize;

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			if (overlaps[i].hEvent) CloseHandle(overlaps[i].hEvent);
		}

		totalCnt += m_batchSize;
	}

	uint64_t testCnt = 0;
	for (uint8_t i = 0; i < workThreadNum; i++)
	{
		testCnt += completeCnt[i];
	}
	if (testCnt != totalCnt)
	{
		std::cout << "not complete all" << std::endl;
		std::cout << testCnt << std::endl;
		std::cout << totalCnt << std::endl;
	}
	else std::cout << "complete all" << std::endl;

	CloseHandle(iocp);
	CloseHandle(fileHandler);
}

void ReadingTask::IOCPRead()
{
	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<int64_t> distribute(0, m_blockNum - 1);

	HANDLE fileHandler = CreateFileA(m_fileName.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (fileHandler == INVALID_HANDLE_VALUE)
	{
		std::cout << "Create Source Handle failed" << std::endl;
		std::cout << GetLastError() << std::endl;
		CloseHandle(fileHandler);
		exit(-1);
	}

	HANDLE iocp = CreateIoCompletionPort(fileHandler, NULL, 0, 0);

	ULONG_PTR threadExitKey = 1;
	uint8_t workThreadNum = 4;
	std::vector<OVERLAPPED_ENTRY> overlapEntrys(m_batchSize);
	std::vector<OVERLAPPED> overlaps(m_batchSize);
	std::vector<std::unique_ptr<uint8_t[]>> buffers;
	std::vector<std::thread> workThreads;
	std::vector<std::uint64_t> completeCnt(workThreadNum, 0);

	for (uint8_t i = 0; i < m_batchSize; i++)
	{
		buffers.emplace_back(std::make_unique<uint8_t[]>(m_blockSizeInBytes));
	}

	auto WorkThreadFunc = [iocp, this, threadExitKey, &completeCnt] (uint8_t tid) {
		int cnt = 0;
		while (1)
		{
			LPOVERLAPPED overlap;
			ULONG_PTR completionKey = 0;
			DWORD readBytes = 0;

			auto res = GetQueuedCompletionStatus(iocp, &readBytes, &completionKey, &overlap, INFINITE);

			if (completionKey == threadExitKey)
			{
				return 0;
			}

			if (!res)
			{
				std::cout << GetLastError() << std::endl;
				std::cout << "GetQueuedCompletionStatus error " << std::endl;
				exit(-1);
			}
			
			if (readBytes == this->m_blockSizeInBytes)
			{
				completeCnt[tid] += 1;
			}
		}
	};

	//auto WorkThreadFunc = [iocp, this] {
	//	int cnt = 0;
	//	while (1)
	//	{
	//		OVERLAPPED* overlap = nullptr;
	//		ULONG_PTR completionKey = 0;
	//		DWORD transferBytes = 0;
	//		GetQueuedCompletionStatus(iocp, &transferBytes, &completionKey, &overlap, INFINITE);

	//		if (transferBytes != this->m_blockSizeInBytes)
	//		{
	//			std::cout << "GetQueuedCompletionStatus error " << std::endl;
	//		}

	//		cnt++;

	//		if (cnt == this->m_batchSize) break;

	//	}
	//};

	uint64_t totalCnt = 0;
	while (m_elapsedTime < m_testTime)
	{
		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			std::uint64_t startIndex = distribute(gen) * m_blockSizeInBytes;
			overlaps[i].Offset = startIndex & UINT32_MAX;
			overlaps[i].OffsetHigh = startIndex >> 32;
			overlaps[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			overlaps[i].Internal = 0;
			overlaps[i].InternalHigh = 0;
			overlapEntrys[i].dwNumberOfBytesTransferred = m_batchSize;
			overlapEntrys[i].lpOverlapped = &overlaps[i];
			overlapEntrys[i].lpCompletionKey = 0;
			overlapEntrys[i].Internal = 0;
		}

		for (uint8_t i = 0; i < workThreadNum; i++)
		{
			workThreads.emplace_back(WorkThreadFunc, i);
		}

		auto startTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			auto res = ReadFile(fileHandler, buffers[i].get(), m_blockSizeInBytes, NULL, &overlaps[i]);

			if (!res && GetLastError() != ERROR_IO_PENDING)
			{
				std::cout << "File read failed " << std::endl;
				std::cout << GetLastError() << std::endl;
				CloseHandle(fileHandler);
				exit(-1);
			}
		}

		auto endIOSendTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < workThreadNum; i++)
		{
			OVERLAPPED overlap;
			PostQueuedCompletionStatus(iocp, 0, threadExitKey, &overlap);
		}

		for (uint8_t i = 0; i < workThreadNum; i++)
		{
			if(workThreads[i].joinable())
				workThreads[i].join();
		}
		workThreads.clear();

		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
		auto sendDuration = std::chrono::duration_cast<std::chrono::microseconds>(endIOSendTime - startTime);
		auto waitDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - endIOSendTime);
		m_queryLatency.emplace_back(duration.count());
		m_sendLatency.emplace_back(sendDuration.count());
		m_waitLatency.emplace_back(waitDuration.count());
		m_elapsedTime += duration.count();
		m_readTotalBytes += m_blockSizeInBytes * m_batchSize;

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			if (overlaps[i].hEvent) CloseHandle(overlaps[i].hEvent);
		}

		totalCnt += m_batchSize;
	}

	uint64_t testCnt = 0;
	for (uint8_t i = 0; i < workThreadNum; i++)
	{
		testCnt += completeCnt[i];
	}
	if (testCnt != totalCnt)
	{
		std::cout << "not complete all" << std::endl;
		std::cout << testCnt << std::endl;
		std::cout << totalCnt << std::endl;
	}
	else std::cout << "complete all" << std::endl;

	CloseHandle(iocp);
	CloseHandle(fileHandler);

}

void ReadingTask::AsyncRead()
{
	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<int64_t> distribute(0, m_blockNum - 1);

	HANDLE fileHandler = CreateFileA(m_fileName.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (fileHandler == INVALID_HANDLE_VALUE)
	{
		std::cout << "Create Source Handle failed" << std::endl;
		std::cout << GetLastError() << std::endl;
		CloseHandle(fileHandler);
		exit(-1);
	}

	std::vector<HANDLE> handlers(m_batchSize);
	std::vector<OVERLAPPED> overlaps(m_batchSize);
	std::vector<std::unique_ptr<uint8_t[]>> buffers;
	for (uint8_t i = 0; i < m_batchSize; i++)
	{
		buffers.emplace_back(std::make_unique<uint8_t[]>(m_blockSizeInBytes));
	}

	while (m_elapsedTime < m_testTime)
	{
		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			std::uint64_t startIndex = distribute(gen) * m_blockSizeInBytes;
			overlaps[i].Offset = startIndex & UINT32_MAX;
			overlaps[i].OffsetHigh = startIndex >> 32;
			overlaps[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			overlaps[i].Internal = 0;
			overlaps[i].InternalHigh = 0;
			handlers[i] = overlaps[i].hEvent;

		}
		auto startTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			auto res = ReadFile(fileHandler, buffers[i].get(), m_blockSizeInBytes, NULL, &overlaps[i]);

			if (!res && GetLastError() != ERROR_IO_PENDING)
			{
				std::cout << "File read failed " << std::endl;
				std::cout << GetLastError() << std::endl;
				CloseHandle(fileHandler);
				exit(-1);
			}
		}

		auto endIOSendTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < m_batchSize; i += MAXIMUM_WAIT_OBJECTS)
		{
			DWORD remainRequest = m_batchSize - i;
			DWORD waitCnt = remainRequest > MAXIMUM_WAIT_OBJECTS ? MAXIMUM_WAIT_OBJECTS : remainRequest;
			DWORD waitRes = WaitForMultipleObjects(waitCnt, &handlers[i], TRUE, INFINITE);

			if (waitRes == WAIT_FAILED)
			{
				std::cout << "wait failed " << std::endl;
				std::cout << GetLastError() << std::endl;
				CloseHandle(fileHandler);
				exit(-1);
			}
		}

		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
		auto sendDuration = std::chrono::duration_cast<std::chrono::microseconds>(endIOSendTime - startTime);
		auto waitDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - endIOSendTime);
		m_queryLatency.emplace_back(duration.count());
		m_sendLatency.emplace_back(sendDuration.count());
		m_waitLatency.emplace_back(waitDuration.count());
		m_elapsedTime += duration.count();
		m_readTotalBytes += m_blockSizeInBytes * m_batchSize;

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			if (overlaps[i].hEvent) CloseHandle(overlaps[i].hEvent);
		}
	}

	CloseHandle(fileHandler);
}


void ReadingTask::AsyncLockRead()
{
	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<int64_t> distribute(0, m_blockNum - 1);

	HANDLE fileHandler = CreateFileA(m_fileName.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (fileHandler == INVALID_HANDLE_VALUE)
	{
		std::cout << "Create Source Handle failed" << std::endl;
		std::cout << GetLastError() << std::endl;
		CloseHandle(fileHandler);
		exit(-1);
	}

	std::vector<HANDLE> handlers(m_batchSize);
	std::vector<OVERLAPPED> overlaps(m_batchSize);
	std::vector<std::unique_ptr<uint8_t[]>> buffers;
	for (uint8_t i = 0; i < m_batchSize; i++)
	{
		buffers.emplace_back(std::make_unique<uint8_t[]>(m_blockSizeInBytes));
	}

	auto setRes = SetFileIoOverlappedRange(fileHandler, (PUCHAR)&overlaps[0], m_batchSize * sizeof(OVERLAPPED));

	if (!setRes)
	{
		std::cout << GetLastError() << std::endl;
		std::cout << "setFileOverLapped error" << std::endl;
		exit(-1);
	}

	while (m_elapsedTime < m_testTime)
	{
		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			std::uint64_t startIndex = distribute(gen) * m_blockSizeInBytes;
			overlaps[i].Offset = startIndex & UINT32_MAX;
			overlaps[i].OffsetHigh = startIndex >> 32;
			overlaps[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			overlaps[i].Internal = 0;
			overlaps[i].InternalHigh = 0;
			handlers[i] = overlaps[i].hEvent;

		}
		auto startTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			auto res = ReadFile(fileHandler, buffers[i].get(), m_blockSizeInBytes, NULL, &overlaps[i]);

			if (!res && GetLastError() != ERROR_IO_PENDING)
			{
				std::cout << "File read failed " << std::endl;
				std::cout << GetLastError() << std::endl;
				CloseHandle(fileHandler);
				exit(-1);
			}
		}

		auto endIOSendTime = std::chrono::high_resolution_clock::now();

		for (uint8_t i = 0; i < m_batchSize; i += MAXIMUM_WAIT_OBJECTS)
		{
			DWORD remainRequest = m_batchSize - i;
			DWORD waitCnt = remainRequest > MAXIMUM_WAIT_OBJECTS ? MAXIMUM_WAIT_OBJECTS : remainRequest;
			DWORD waitRes = WaitForMultipleObjects(waitCnt, &handlers[i], TRUE, INFINITE);

			if (waitRes == WAIT_FAILED)
			{
				std::cout << "wait failed " << std::endl;
				std::cout << GetLastError() << std::endl;
				CloseHandle(fileHandler);
				exit(-1);
			}
		}

		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
		auto sendDuration = std::chrono::duration_cast<std::chrono::microseconds>(endIOSendTime - startTime);
		auto waitDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - endIOSendTime);
		m_queryLatency.emplace_back(duration.count());
		m_sendLatency.emplace_back(sendDuration.count());
		m_waitLatency.emplace_back(waitDuration.count());
		m_elapsedTime += duration.count();
		m_readTotalBytes += m_blockSizeInBytes * m_batchSize;

		for (uint8_t i = 0; i < m_batchSize; i++)
		{
			if (overlaps[i].hEvent) CloseHandle(overlaps[i].hEvent);
		}
	}

	CloseHandle(fileHandler);
}

void ReadingTask::Run()
{
	switch (m_readMethod)
	{
	case ReadMethod::AsyncLockRead:
		AsyncLockRead();
		break;
	case ReadMethod::AsyncRead:
		AsyncRead();
		break;
	case ReadMethod::IOCPRead:
		IOCPRead();
		break;
	case ReadMethod::IOCPLockRead:
		IOCPLockRead();
		break;
	default:
		break;
	}
}
