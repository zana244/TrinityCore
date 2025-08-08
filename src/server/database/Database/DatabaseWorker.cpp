/*
 * This file is part of the TrinityCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "DatabaseWorker.h"
#include "SQLOperation.h"
#include "ProducerConsumerQueue.h"
#include "Tracy.hpp"
#include <cstddef>
#include <map>
#include <mutex>
#include <memory>

static std::map<std::string, std::size_t> curId;
static std::mutex curIdMutex; // todo: probably not needed
DatabaseWorker::DatabaseWorker(ProducerConsumerQueue<SQLOperation*>* newQueue, MySQLConnection* connection, std::string name)
{
    _connection = connection;
    _queue = newQueue;
    {
        std::scoped_lock lock(curIdMutex);
        _name = fmt::format("{}{}", name, curId[name]++);
    }
    _workerThread = std::thread(&DatabaseWorker::WorkerThread, this);
}

DatabaseWorker::~DatabaseWorker()
{
    _queue->CancelGraceful();

    _workerThread.join();
    ASSERT(_queue->Empty());
    TC_LOG_INFO("server.database", "Database worker {} gracefully shut down", _name);
}

void DatabaseWorker::WorkerThread()
{
    if (!_queue)
        return;

    ZoneScopedN("DatabaseWorker::WorkerThread");
    tracy::SetThreadName(_name.c_str());
    for (;;)
    {
        SQLOperation* operation = nullptr;
        if (!_queue->WaitAndPop(operation) || !operation)
            return;

        operation->SetConnection(_connection);
        operation->call();
        delete operation;
    }
}
