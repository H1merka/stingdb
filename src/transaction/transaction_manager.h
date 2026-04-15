#pragma once

#include "transaction/transaction.h"
#include "transaction/log_manager.h"
#include <mutex>
#include <unordered_map>
#include <memory>
#include <atomic>

namespace stingdb::transaction {

/**
 * TransactionManager: Оркестратор транзакционных переходов (BEGIN, COMMIT, ABORT).
 * Управляет выдачей новых TxnId, эпохами (EpochManager) и регистрацией в WAL.
 */
class TransactionManager {
public:
    explicit TransactionManager(LogManager* log_manager) : log_manager_(log_manager) {}

    // Начало новой транзакции (выдача TxnId, вход в текущую эпоху).
    Transaction* Begin(IsolationLevel iso_level = IsolationLevel::SNAPSHOT_ISOLATION) {
        TxnId new_id = next_txn_id_.fetch_add(1, std::memory_order_relaxed);
        
        auto txn = std::make_unique<Transaction>(new_id, iso_level);
        txn->EnterReadEpoch(&epoch_manager_);

        // Выполняем добавление записи BEGIN в лог
        LogRecord begin_log(new_id, 0, LogRecordType::BEGIN);
        TypeEpoch begin_lsn = log_manager_->AppendLogRecord(&begin_log);
        txn->AppendWriteSet(begin_lsn);

        std::lock_guard<std::mutex> lock(txn_map_mutex_);
        Transaction* raw_ptr = txn.get();
        txn_map_.emplace(new_id, std::move(txn));
        return raw_ptr;
    }

    // Фиксация транзакции. 
    void Commit(Transaction* txn) {
        if (txn->GetState() == TransactionState::ABORTED) {
            return;
        }

        // Логирование COMMIT
        LogRecord commit_log(txn->GetTxnId(), 0, LogRecordType::COMMIT);
        log_manager_->AppendLogRecord(&commit_log);

        // Принудительный сброс на диск (Force WAL) для долговечности (Durability)
        log_manager_->Flush();

        // Снимаем транзакцию с трекинга эпохи
        txn->Commit();

        // Transaction будет удален из txn_map_ во время сборки мусора (GC)
    }

    // Откат транзакции.
    void Abort(Transaction* txn) {
        if (txn->GetState() == TransactionState::COMMITTED) {
            return;
        }

        // В итерации 12 тут будет проход по UNDO логам (обратный ход по LSN)
        // пока мы только логируем факт отката
        LogRecord abort_log(txn->GetTxnId(), 0, LogRecordType::ABORT);
        log_manager_->AppendLogRecord(&abort_log);

        txn->Abort();
    }

private:
   std::atomic<TxnId> next_txn_id_{1};
   LogManager* log_manager_;
   EpochManager epoch_manager_;

   std::mutex txn_map_mutex_;
   std::unordered_map<TxnId, std::unique_ptr<Transaction>> txn_map_;
};

} // namespace stingdb::transaction