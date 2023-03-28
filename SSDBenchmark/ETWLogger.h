#pragma once

#include <windows.h>
#include <stdio.h>
#include <evntprov.h>
#include <winmeta.h>
#include <string>
#include <atomic>

#include "HashStoreETWTracing.h"

#define LOG_ETW_EVENT(Event, ...)                                                                 \
{                                                                                                 \
    EVENT_DATA_DESCRIPTOR arrDataDescriptors[] = __VA_ARGS__;                                     \
    ETWLogger::GetInstance().LogEvent(Event, arrDataDescriptors, _countof(arrDataDescriptors));   \
}

class ETWLogger
{

public:
    static ETWLogger& GetInstance()
    {
        static ETWLogger instance;
        return instance;
    }

    ~ETWLogger();

    // Judge if the provider is enabled by keyword.
    bool IsEventTracingEnabled();

    // Judge if the event is enabled by session.
    static bool IsEventLoggingEnabled(PCEVENT_DESCRIPTOR eventDescriptor);

    // Functions to write ETW tracing log for every event.
    static void LogReadMemoryRecord(const std::string& strRequestGuid, std::uint64_t address, std::uint32_t size);

    // Functions to write ETW tracing log for every Request Processing Event.
    static void LogRequestProcessEvent(const PCEVENT_DESCRIPTOR eventDescriptor, const std::string& strRequestGuid);

    // Log ReadDataSummaryInfo
    static void LogRequestProcessSummaryEvent(const PCEVENT_DESCRIPTOR eventDescriptor, const std::string& strRequestGuid, const std::string& message);

private:
    explicit ETWLogger();

    ETWLogger(const ETWLogger&);
    const ETWLogger& operator=(const ETWLogger&);

    bool IsEventEnabled(PCEVENT_DESCRIPTOR eventDescriptor)
    {
        if (m_etwRegistration != NULL)
        {
            return !!EventEnabled(m_etwRegistration, eventDescriptor);
        }

        return false;
    }

    void LogEvent(PCEVENT_DESCRIPTOR eventDescriptor,
        const PEVENT_DATA_DESCRIPTOR userData = nullptr,
        size_t userDataCount = 0);

    std::atomic<REGHANDLE> m_etwRegistration;
};

