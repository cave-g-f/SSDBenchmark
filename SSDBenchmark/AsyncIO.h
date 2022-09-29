#pragma once
#include <memory>
#include <Windows.h>

class AsyncIORequest
{
public:
	AsyncIORequest(
		std::uint64_t readBytes,
		OVERLAPPED& overlap) :
		m_overlap(overlap),
		m_readBytes(readBytes)
	{
		m_buffer = std::make_unique<std::uint8_t[]>(readBytes);
	}

	std::uint8_t* GetBuffer()
	{
		return m_buffer.get();
	}

	OVERLAPPED& GetOverlap()
	{
		return m_overlap;
	}

	bool IsReadSucceeded() const
	{
		return (m_errorCode == ERROR_SUCCESS && m_returnBytes == m_readBytes) || m_errorCode == ERROR_IO_PENDING;
	}

public:
	DWORD m_returnBytes;
	std::uint64_t m_readBytes;
	std::uint32_t m_errorCode;

private:
	std::unique_ptr<std::uint8_t[]> m_buffer;
	OVERLAPPED m_overlap;
};