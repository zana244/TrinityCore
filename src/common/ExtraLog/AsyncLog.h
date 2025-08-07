#pragma once

#include "Define.h"

#include <string>
#include <unordered_map>

struct AsyncLogData {
    std::string query;
    uint64 createTime;
};

struct AsyncLogTotals
{
    std::string Name;
    uint64 Count = 0;
    uint64 Time = 0;
};
TC_COMMON_API std::unordered_map<uint64, AsyncLogData> GetAsyncLogData();
TC_COMMON_API void RemoveAsyncLogEntry(uint64& entry);
TC_COMMON_API uint64 CreateAsyncLogEntry(std::string query);
TC_COMMON_API std::unordered_map<std::string, AsyncLogTotals> GetAsyncLogTotals();
