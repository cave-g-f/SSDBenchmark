#include "ReadingTask.h"

ReadingTask::ReadingTask(
	std::uint32_t readKeyNumberPerQuery,
	std::uint8_t tId)
	: m_readKeyNumberPerQuery(readKeyNumberPerQuery)
	, m_tId(tId)
{
	m_elapsedTime = 0;
	m_readTotalBytes = 0;
	m_fileName = Config::get().getValStr(configName::TestFile);
	m_fileSize = Config::get().getValUint8(configName::FileSize);
	m_testTime = (Config::get().getValUint64(configName::RTime)) * 1000 * 1000;
	m_blockSize = Config::get().getValUint8(configName::RBlockSize);
	m_batchSize = Config::get().getValUint64(configName::BatchSize);
	m_blockNum = (static_cast<uint64_t>(m_fileSize) << 20) / m_blockSize;
	m_blockSizeInBytes = (DWORD)(m_blockSize) << 10;
	m_readMethod = (ReadMethod)Config::get().getValUint8(configName::ReadMethod);
	m_readSpeed = Config::get().getValUint64(configName::ReadSpeed) / Config::get().getValUint8(configName::ThreadsNum);
	if (m_readSpeed != 0) m_queryTime = 1000 / m_readSpeed;

	m_threadNumber = Config::get().getValUint8(configName::ThreadNumberForSSDRead);
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

	for (uint64_t i = 0; i < m_batchSize; i++)
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
		for (uint64_t i = 0; i < m_batchSize; i++)
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

		for (uint64_t i = 0; i < m_batchSize; i++)
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

		for (uint64_t i = 0; i < m_batchSize; i++)
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

	for (uint64_t i = 0; i < m_batchSize; i++)
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

	uint64_t totalCnt = 0;
	while (m_elapsedTime < m_testTime)
	{
		for (uint64_t i = 0; i < m_batchSize; i++)
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

		for (uint64_t i = 0; i < m_batchSize; i++)
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

		for (uint64_t i = 0; i < m_batchSize; i++)
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

void CALLBACK ReadingTask::MultiRead(PTP_CALLBACK_INSTANCE, void* pContext, PTP_WORK)
{
	ThreadDataBag* threadDataBag = reinterpret_cast<ThreadDataBag*>(pContext);
	ReadingTask* readingTask = threadDataBag->m_readingTask;

	for (uint64_t i = threadDataBag->m_startIndex; i < threadDataBag->m_endIndex; i++)
	{
		auto res = ReadFile(readingTask->m_file, readingTask->m_buffer[i].get(), readingTask->m_blockSizeInBytes, NULL, &threadDataBag->m_overlappedInfos[i]);

		if (!res && GetLastError() != ERROR_IO_PENDING)
		{
			std::cout << "File read failed " << std::endl;
			std::cout << GetLastError() << std::endl;
			CloseHandle(readingTask->m_file);
			exit(-1);
		}
	}

	for (uint64_t i = threadDataBag->m_startIndex; i < threadDataBag->m_endIndex; i += MAXIMUM_WAIT_OBJECTS)
	{
		DWORD remainRequest = threadDataBag->m_endIndex - i;
		DWORD waitCnt = remainRequest > MAXIMUM_WAIT_OBJECTS ? MAXIMUM_WAIT_OBJECTS : remainRequest;
		DWORD waitRes = WaitForMultipleObjects(waitCnt, &threadDataBag->m_handlesInfos[i], TRUE, INFINITE);

		if (waitRes == WAIT_FAILED)
		{
			std::cout << "wait failed " << std::endl;
			std::cout << GetLastError() << std::endl;
			CloseHandle(readingTask->m_file);
			exit(-1);
		}
	}

	return;
}

void ReadingTask::AsyncRead()
{
	std::random_device dv;
	std::mt19937 gen(dv());
	std::uniform_int_distribution<int64_t> distribute(0, m_blockNum - 1);

	m_file = CreateFileA(m_fileName.c_str(), GENERIC_READ | FILE_READ_ATTRIBUTES, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_NO_BUFFERING | FILE_FLAG_OVERLAPPED, NULL);
	if (m_file == INVALID_HANDLE_VALUE)
	{
		std::cout << "Create Source Handle failed" << std::endl;
		std::cout << GetLastError() << std::endl;
		CloseHandle(m_file);
		exit(-1);
	}
	std::vector<HANDLE> handlers(m_batchSize);
	std::vector<OVERLAPPED> overlaps(m_batchSize);
	m_buffer.reserve(m_batchSize);
	for (uint64_t i = 0; i < m_batchSize; i++)
	{
		m_buffer.emplace_back(std::make_unique<uint8_t[]>(m_blockSizeInBytes));
	}

	uint64_t prevReadBytes = 0;
	uint64_t prevElapsedTime = 0;
	while (m_elapsedTime < m_testTime)
	{
		for (uint64_t i = 0; i < m_batchSize; i++)
		{
			std::uint64_t startIndex = distribute(gen) * m_blockSizeInBytes;
			overlaps[i].Offset = startIndex & UINT32_MAX;
			overlaps[i].OffsetHigh = startIndex >> 32;
			overlaps[i].hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
			overlaps[i].Internal = 0;
			overlaps[i].InternalHigh = 0;
			handlers[i] = overlaps[i].hEvent;
		}

		uint64_t queryNumForThread = m_batchSize / m_threadNumber;

		std::vector<ThreadDataBag> threadParameters;
		std::vector<PTP_WORK> multiWorks;

		threadParameters.reserve(m_threadNumber);
		multiWorks.reserve(m_threadNumber - 1);

		auto startTime = std::chrono::high_resolution_clock::now();

		size_t startIndex = 0;
		for (uint8_t i = 0; i < m_threadNumber - 1; i++)
		{
			threadParameters.emplace_back(
				startIndex,
				startIndex + queryNumForThread,
				&handlers[0],
				&overlaps[0],
				this
			);

			PTP_WORK workItem = CreateThreadpoolWork(MultiRead, &threadParameters.back(), nullptr);

			if (workItem == nullptr)
			{
				std::cout << "Can't create workItem for multThread" << std::endl;
				std::cout << GetLastError() << std::endl;
				threadParameters.pop_back();
				continue;
			}

			SubmitThreadpoolWork(workItem);
			multiWorks.push_back(workItem);
			startIndex += queryNumForThread;
		}

		threadParameters.emplace_back(
			startIndex,
			m_batchSize,
			&handlers[0],
			&overlaps[0],
			this
		);

		MultiRead(nullptr, &threadParameters.back(), nullptr);

		for (auto& workItem : multiWorks)
		{
			WaitForThreadpoolWorkCallbacks(workItem, false);
			CloseThreadpoolWork(workItem);
		}

		auto endTime = std::chrono::high_resolution_clock::now();
		auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
		auto sendDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
		auto waitDuration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - endTime);
		m_queryLatency.emplace_back(duration.count());
		m_sendLatency.emplace_back(sendDuration.count());
		m_waitLatency.emplace_back(waitDuration.count());
		m_readTotalBytes += m_blockSizeInBytes * m_batchSize;
		m_elapsedTime += duration.count();

		for (uint64_t i = 0; i < m_batchSize; i++)
		{
			if (overlaps[i].hEvent) CloseHandle(overlaps[i].hEvent);
		}

		if (m_readSpeed != 0 && (m_readTotalBytes - prevReadBytes >= (1 << 20)) && (m_elapsedTime - prevElapsedTime < (m_queryTime * 1000)))
		{
			uint64_t sleepTime = m_queryTime * 1000 - m_elapsedTime + prevElapsedTime;
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime / 1000));
			m_elapsedTime += sleepTime;
			prevReadBytes = m_readTotalBytes;
			prevElapsedTime = m_elapsedTime;
		}

	}

	CloseHandle(m_file);
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
	for (uint64_t i = 0; i < m_batchSize; i++)
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
		for (uint64_t i = 0; i < m_batchSize; i++)
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

		for (uint64_t i = 0; i < m_batchSize; i++)
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

		for (uint64_t i = 0; i < m_batchSize; i += MAXIMUM_WAIT_OBJECTS)
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

		for (uint64_t i = 0; i < m_batchSize; i++)
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
