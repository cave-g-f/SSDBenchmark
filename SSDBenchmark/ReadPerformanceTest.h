#pragma once

#include <iostream>
#include <random>
#include <Windows.h>
#include <thread>

#include "Config.h"
#include "AsyncRead.h"
#include "IOCPRead.h"
#include "IODepthRead.h"
#include "SyncRead.h"
#include "HashStoreETWTracing.h"

struct TestStatisticBag
{
	double avgQueryLatency;
	double queryLatency50;
	double queryLatency90;
	double queryLatency95;
	double queryLatency99;
	double queryLatency999;
	double queryLatencyMax;
	double avgSendLatency;
	double sendLatency50;
	double sendLatency90;
	double sendLatency95;
	double sendLatency99;
	double sendLatency999;
	double sendLatencyMax;
	double avgWaitLatency;
	double waitLatency50;
	double waitLatency90;
	double waitLatency95;
	double waitLatency99;
	double waitLatency999;
	double waitLatencyMax;
	std::uint64_t readTotalBytes = 0;
	std::uint64_t requestCnt = 0;

	std::string print()
	{
		std::string latencyStr = "";
		latencyStr += "-----total latency-----\n";
		latencyStr += "avg: " + std::to_string(avgQueryLatency) + "\n";
		latencyStr += "50th: " + std::to_string(queryLatency50) + "\n";
		latencyStr += "90th: " + std::to_string(queryLatency90) + "\n";
		latencyStr += "95th: " + std::to_string(queryLatency95) + "\n";
		latencyStr += "99th: " + std::to_string(queryLatency99) + "\n";
		latencyStr += "99.9th: " + std::to_string(queryLatency999) + "\n";
		latencyStr += "max: " + std::to_string(queryLatencyMax) + "\n";
		latencyStr += "-----send latency------\n";
		latencyStr += "avg: " + std::to_string(avgSendLatency) + "\n";
		latencyStr += "50th: " + std::to_string(sendLatency50)+ "\n";
		latencyStr += "90th: " + std::to_string(sendLatency90) + "\n";
		latencyStr += "95th: " + std::to_string(sendLatency95) + "\n";
		latencyStr += "99th: " + std::to_string(sendLatency99) + "\n";
		latencyStr += "99.9th: " + std::to_string(sendLatency999) + "\n";
		latencyStr += "max: " + std::to_string(sendLatencyMax) + "\n";
		latencyStr += "-----wait latency------\n";
		latencyStr += "avg: " + std::to_string(avgWaitLatency) + "\n";
		latencyStr += "50th: " + std::to_string(waitLatency50) + "\n";
		latencyStr += "90th: " + std::to_string(waitLatency90) + "\n";
		latencyStr += "95th: " + std::to_string(waitLatency95) + "\n";
		latencyStr += "99th: " + std::to_string(waitLatency99) + "\n";
		latencyStr += "99.9th: " + std::to_string(waitLatency999) + "\n";
		latencyStr += "max: " + std::to_string(waitLatencyMax) + "\n";

		return latencyStr;
	}
};

class ReadPerformanceTest
{
public:
	ReadPerformanceTest();
	void runReadBenchmark();
	~ReadPerformanceTest();

private:
	void prepareTestFile();
	void start(std::uint8_t tId, TestStatisticBag& latencyBag);
	void SetPrivilege(LPCWSTR privilegeName);
	void setAndDumpLatencyFile(std::uint8_t tId, ReadAlgoBase* readAlgo, TestStatisticBag& statisticBag);

private:
	std::uint64_t m_testTime;
	std::uint64_t m_threadNumber;
	std::string m_fileName;
	std::uint8_t m_fileSize;
	std::uint64_t m_bufferLenInBytes;
	bool m_memoryLock;
	bool m_printRawLatency;
	Config::ReadMethod m_readMethod;
	std::vector<std::ofstream> m_latencyFileHander;
	std::vector<std::ofstream> m_rawLatencyFileHander;
	std::uint64_t m_latencyCounterDuration;
	std::uint64_t m_qps;
	std::uint64_t m_timePerQuery;
	std::atomic<uint64_t> m_ioDepth = 0;
	std::uint64_t m_ioDepthThreshold = 0;
};

