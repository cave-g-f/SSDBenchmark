#include "ETWLogger.h"

#include <iostream>

ETWLogger::ETWLogger() : m_etwRegistration(NULL)
{
    REGHANDLE etwRegister = NULL;
    if (EventRegister(&HashStoreEtwTracingProviderGuid, nullptr, nullptr, &etwRegister) != ERROR_SUCCESS)
    {
        std::cout << "Cannot register ETW tracing provider." << std::endl;
        m_etwRegistration = NULL;
    }

    m_etwRegistration.store(etwRegister);
}

ETWLogger::~ETWLogger()
{
    if (m_etwRegistration != NULL)
    {
        EventUnregister(m_etwRegistration);
        m_etwRegistration = NULL;
    }
}

void ETWLogger::LogEvent(PCEVENT_DESCRIPTOR eventDescriptor,
    const PEVENT_DATA_DESCRIPTOR userData,
    size_t userDataCount)
{
    if (m_etwRegistration != NULL)
    {
        EventWrite(m_etwRegistration, eventDescriptor, static_cast<ULONG>(userDataCount), userData);
    }
}

bool ETWLogger::IsEventTracingEnabled()
{
    if (m_etwRegistration != NULL)
    {
        return !!EventProviderEnabled(m_etwRegistration, WINEVENT_LEVEL_INFO, HASH_STORE);
    }

    return false;
}

bool ETWLogger::IsEventLoggingEnabled(PCEVENT_DESCRIPTOR eventDescriptor)
{
    return ETWLogger::GetInstance().IsEventEnabled(eventDescriptor);
}

void ETWLogger::LogReadMemoryRecord(const std::string& strRequestGuid, const std::uint64_t address, std::uint32_t size)
{
    if (!IsEventLoggingEnabled(&ReadMemoryRecordEvent) || strRequestGuid.size() == 0)
    {
        return;
    }

    LOG_ETW_EVENT(&ReadMemoryRecordEvent,
        {
            { (ULONGLONG)strRequestGuid.c_str(), (ULONG)strRequestGuid.size() + 1 },
            { (ULONGLONG)&address, (ULONG)sizeof(std::uint64_t) },
            { (ULONGLONG)&size, (ULONG)sizeof(std::uint32_t) }
        });
}

void ETWLogger::LogRequestProcessEvent(const PCEVENT_DESCRIPTOR eventDescriptor, const std::string& strRequestGuid)
{
    if (!IsEventLoggingEnabled(eventDescriptor) || strRequestGuid.size() == 0)
    {
        return;
    }

    LOG_ETW_EVENT(eventDescriptor,
        {
            (ULONGLONG)strRequestGuid.c_str(), (ULONG)strRequestGuid.size() + 1
        });
}

void ETWLogger::LogRequestProcessSummaryEvent(const PCEVENT_DESCRIPTOR eventDescriptor, const std::string& strRequestGuid, const std::string& message)
{
    if (!IsEventLoggingEnabled(eventDescriptor) || strRequestGuid.size() == 0)
    {
        return;
    }

    LOG_ETW_EVENT(eventDescriptor,
        {
            { (ULONGLONG)strRequestGuid.c_str(), (ULONG)strRequestGuid.size() + 1 },
            { (ULONGLONG)message.c_str(), (ULONG)message.size() + 1 }
        });
}