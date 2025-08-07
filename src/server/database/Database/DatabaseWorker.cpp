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

DatabaseWorker::DatabaseWorker(ProducerConsumerQueue<SQLOperation*>* newQueue, MySQLConnection* connection, std::string name)
{
    _connection = connection;
    _queue = newQueue;
    _cancelationToken = false;
    _workerThread = std::thread(&DatabaseWorker::WorkerThread, this);
    _name = name;
}

DatabaseWorker::~DatabaseWorker()
{
    _cancelationToken = true;

    _queue->Cancel();

    _workerThread.join();
}

std::map<std::string, std::size_t> curId;
std::mutex curIdMutex;
void DatabaseWorker::WorkerThread()
{
    if (!_queue)
        return;

    std::string name = [&]() {
        std::scoped_lock lock(curIdMutex);
        return fmt::format("{} Database {}", _name, curId[_name]++);
    }();

    ZoneScopedN("DatabaseWorker::WorkerThread");
    tracy::SetThreadName(name.c_str());
    for (;;)
    {
        SQLOperation* operation = nullptr;

        _queue->WaitAndPop(operation);

        if (_cancelationToken || !operation)
            return;

        operation->SetConnection(_connection);
        operation->call();
        delete operation;
    }
}
