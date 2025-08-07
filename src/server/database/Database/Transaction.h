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

#ifndef _TRANSACTION_H
#define _TRANSACTION_H

#include "Define.h"
#include "AsyncLog.h"
#include "DatabaseEnvFwd.h"
#include "SQLOperation.h"
#include "StringFormat.h"
#include <functional>
#include <mutex>
#include <vector>

/*! Transactions, high level class. */
class TC_DATABASE_API TransactionBase
{
    friend class TransactionTask;
    friend class MySQLConnection;

    template <typename T>
    friend class DatabaseWorkerPool;

    public:
        TransactionBase(std::string const& name) : _cleanedUp(false), m_name(name) { }
        virtual ~TransactionBase() { Cleanup(); }

        void Append(char const* sql);
        template<typename... Args>
        void PAppend(Trinity::FormatString<Args...> sql, Args&&... args)
        {
            this->Append(Trinity::StringFormat(sql, std::forward<Args>(args)...).c_str());
        }

        std::size_t GetSize() const { return m_queries.size(); }

        std::string GetName() const { return m_name; }
    protected:
        void AppendPreparedStatement(PreparedStatementBase* statement);
        void Cleanup();
        std::vector<SQLElementData> m_queries;
        std::string m_name;
    private:
        bool _cleanedUp;
};

template<typename T>
class Transaction : public TransactionBase
{
public:
    using TransactionBase::TransactionBase;
    using TransactionBase::Append;
    void Append(PreparedStatement<T>* statement)
    {
        this->AppendPreparedStatement(statement);
    }
};

/*! Low level class*/
class TC_DATABASE_API TransactionTask : public SQLOperation
{
    template <class T> friend class DatabaseWorkerPool;
    friend class DatabaseWorker;
    friend class TransactionCallback;

    public:
        TransactionTask(std::shared_ptr<TransactionBase> trans) : m_trans(trans) { }
        ~TransactionTask() { }

        std::string GetName() {
            if (m_trans)
            {
                return m_trans->GetName();
            }
            else
            {
                return "unknown";
            }
        }
    protected:
        bool Execute() override;
        int TryExecute();
        void CleanupOnFailure();

        std::shared_ptr<TransactionBase> m_trans;
        static std::mutex _deadlockLock;
};

class TC_DATABASE_API TransactionWithResultTask : public TransactionTask
{
public:
    TransactionWithResultTask(std::shared_ptr<TransactionBase> trans) : TransactionTask(trans) { }

    TransactionFuture GetFuture() { return m_result.get_future(); }

protected:
    bool Execute() override;

    TransactionPromise m_result;
};

class TC_DATABASE_API TransactionCallback
{
public:
    TransactionCallback(TransactionFuture&& future, std::string const& name) : m_future(std::move(future))
    {

    }
    TransactionCallback(TransactionCallback&& other) noexcept
        : m_future(std::move(other.m_future))
        , m_callback(std::move(other.m_callback))
        , logEntryNo(other.logEntryNo)
    {
        other.logEntryNo = 0;
    }

    TransactionCallback& operator=(TransactionCallback&& other) noexcept {
        m_future = std::move(other.m_future);
        m_callback = std::move(other.m_callback);
        logEntryNo = other.logEntryNo;
        other.logEntryNo = 0;
        return *this;
    }

    void AfterComplete(std::function<void(bool)> callback) &
    {
        m_callback = std::move(callback);
    }

    bool InvokeIfReady();

    TransactionFuture m_future;
    std::function<void(bool)> m_callback;
    uint64 logEntryNo = 0;
};

#endif
