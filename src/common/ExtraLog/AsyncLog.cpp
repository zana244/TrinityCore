#include "AsyncLog.h"

#include <mutex>
#include <chrono>
#include <string>

std::mutex createAsyncLogDataMutex; 
std::unordered_map<uint64, AsyncLogData> logData;
std::unordered_map<std::string, AsyncLogTotals> logTotals;
uint64 curLogDataEntry = 1;

std::unordered_map<uint64, AsyncLogData> GetAsyncLogData()
{
    std::scoped_lock lock(createAsyncLogDataMutex);
    std::unordered_map<uint64, AsyncLogData> logDataCopy = logData;
    return logDataCopy;
}

void RemoveAsyncLogEntry(uint64& entry)
{
    uint64 now = static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count());
    if (entry != 0)
    {
        std::scoped_lock lock(createAsyncLogDataMutex);
        auto itr = logData.find(entry);
        if (itr != logData.end())
        {

            AsyncLogTotals& totals = logTotals[itr->second.query];
            totals.Name           = itr->second.query;
            totals.Count++;
            totals.Time += now - itr->second.createTime;
        }
        logData.erase(entry);
        entry = 0;
    }
}

uint64 CreateAsyncLogEntry(std::string query)
{
    std::scoped_lock lock(createAsyncLogDataMutex);
    uint64 logDataEntry = curLogDataEntry++;
    logData[logDataEntry] =
        AsyncLogData{query, static_cast<uint64>(std::chrono::duration_cast<std::chrono::milliseconds>(
                                                            std::chrono::system_clock::now().time_since_epoch())
                                                            .count())};
    return logDataEntry;
}

std::unordered_map<std::string, AsyncLogTotals> GetAsyncLogTotals() {
    std::scoped_lock lock(createAsyncLogDataMutex);
    std::unordered_map<std::string, AsyncLogTotals> logTotalsCopy = logTotals;
    return logTotalsCopy;
}
