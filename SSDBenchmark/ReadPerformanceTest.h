#pragma once

#include <iostream>
#include <random>
#include <Windows.h>
#include <thread>

#include "Config.h"

struct TestStatisticBag
{
	double avgQueryLatency;
	double queryLatency50;
	double queryLatency90;
	double queryLatency95;
	double queryLatency99;
	double queryLatency999;
	double avgSendLatency;
	double sendLatency50;
	double sendLatency90;
	double sendLatency95;
	double sendLatency99;
	double sendLatency999;
	double avgWaitLatency;
	double waitLatency50;
	double waitLatency90;
	double waitLatency95;
	double waitLatency99;
	double waitLatency999;
	std::uint64_t readTotalBytes;

	void print()
	{
		std::cout << "-----total latency-----" << std::endl;
		std::cout << "avg: " << avgQueryLatency << std::endl;
		std::cout << "50th: " << queryLatency50 << std::endl;
		std::cout << "90th: " << queryLatency90 << std::endl;
		std::cout << "95th: " << queryLatency95 << std::endl;
		std::cout << "99th: " << queryLatency99 << std::endl;
		std::cout << "99.9th: " << queryLatency999 << std::endl;
		std::cout << "-----send latency------" << std::endl;
		std::cout << "avg: " << avgSendLatency << std::endl;
		std::cout << "50th: " << sendLatency50 << std::endl;
		std::cout << "90th: " << sendLatency90 << std::endl;
		std::cout << "95th: " << sendLatency95 << std::endl;
		std::cout << "99th: " << sendLatency99 << std::endl;
		std::cout << "99.9th: " << sendLatency999 << std::endl;
		std::cout << "-----wait latency------" << std::endl;
		std::cout << "avg: " << avgWaitLatency << std::endl;
		std::cout << "50th: " << waitLatency50 << std::endl;
		std::cout << "90th: " << waitLatency90 << std::endl;
		std::cout << "95th: " << waitLatency95 << std::endl;
		std::cout << "99th: " << waitLatency99 << std::endl;
		std::cout << "99.9th: " << waitLatency999 << std::endl;
	}
};

class ReadPerformanceTest
{
public:
	ReadPerformanceTest();
	void runReadBenchmark();

private:
	void prepareTestFile();
	void start(std::uint8_t tId, TestStatisticBag& latencyBag) const;
	void SetPrivilege(LPCWSTR privilegeName);

private:
	std::uint64_t m_testTime;
	std::uint64_t m_threadNumber;
	std::string m_fileName;
	std::uint8_t m_fileSize;
	std::uint64_t m_bufferLenInBytes;
	bool m_memoryLock;
	Config::ReadMethod m_readMethod;
};

