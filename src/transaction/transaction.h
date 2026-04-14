#pragma once

#include "transaction/epoch_manager.h"
#include <vector>

namespace stingdb::transaction {

enum class TransactionState { ACTIVE, COMMITTED, PRE_ABORT, ABORTED };
enum class IsolationLevel { READ_UNCOMMITTED, READ_COMMITTED, REPEATABLE_READ, SERIALIZABLE };

/**
 * Класс Представляет контекст транзакции с поддержкой Timestamp Ordering и MVCC.
 * Вместо того, чтобы брать глобальный мьютекс или единый атомик, который
 * генерирует lock contention, мы берем 'Epoch' (период времени в несколько мс)
 * на котором мы видим данные из Snapshot'a (Read-View).
 */
class Transaction {
public:
    explicit Transaction(TxnId txn_id, IsolationLevel iso_level = IsolationLevel::REPEATABLE_READ)
        : txn_id_(txn_id), isolation_level_(iso_level) {}

    ~Transaction() = default;

    // Входим в эпоху стартаトランзакции (считываем Read-View).
    void EnterReadEpoch(EpochManager* epoch_manager) {
        read_epoch_ = epoch_manager->EnterEpoch();
        manager_ = epoch_manager;
    }

    // Успешный коммит транзакции
    void Commit() {
        state_ = TransactionState::COMMITTED;
        if (manager_) {
            manager_->ExitEpoch(read_epoch_);
        }
    }

    // Откат изменений: применяем UNDO-логи из write_set_
    void Abort() {
        state_ = TransactionState::ABORTED;
        if (manager_) {
            manager_->ExitEpoch(read_epoch_);
        }
        // ... (Rollback WriteSet)
    }

    // Регистрация UNDO/REDO лога
    void AppendWriteSet(TypeEpoch lsn) {
        write_set_.push_back(lsn);
    }

    [[nodiscard]] TxnId GetTxnId() const { return txn_id_; }
    [[nodiscard]] TypeEpoch GetReadEpoch() const { return read_epoch_; }
    [[nodiscard]] TransactionState GetState() const { return state_; }

private:
    TransactionState state_{TransactionState::ACTIVE};
    IsolationLevel isolation_level_;
    TxnId txn_id_;
    TypeEpoch read_epoch_{0}; // Эпоха "чтения" (Visibility scope MVCC)
    TypeEpoch commit_epoch_{0};

    EpochManager* manager_{nullptr};

    // Набор LSN (адресов логов WAL) для быстрого связывания UNDO-списка транзакции (Rollback/Abort).
    std::vector<TypeEpoch> write_set_;
};

} // namespace stingdb::transaction