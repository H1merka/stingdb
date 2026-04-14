#pragma once

#include "common/config.h"
#include "storage/table/slotted_page.h"
#include "transaction/epoch_manager.h"
#include <vector>
#include <string>

namespace stingdb::transaction {

// Тип записи в журнале (Write-Ahead Log)
enum class LogRecordType { 
    INVALID = 0, 
    INSERT = 1, 
    MARKDELETE = 2, 
    APPLYDELETE = 3, 
    UPDATE = 4, 
    BEGIN = 5, 
    COMMIT = 6, 
    ABORT = 7 
};

/**
 * Физиологическое Логирование (Physiological Logging).
 * Мы логируем физические страницы (PageID), но при этом 
 * логируем только *логическое* действие в слоте (например, данные кортежа), 
 * а не полный слепок страницы в 8КБ. Это тысячекратно сокращает размер WAL на диске.
 */
class LogRecord {
public:
    LogRecord() = default;

    // Конструктор для транзакционных маркеров (BEGIN, COMMIT, ABORT)
    LogRecord(TxnId txn_id, TypeEpoch lsn, LogRecordType type)
        : size_(HeaderSize()),
          type_(type),
          txn_id_(txn_id),
          prev_lsn_(0) {}

    // Конструктор для INSERT (содержит логические данные, но физический PageID)
    LogRecord(TxnId txn_id, TypeEpoch lsn, LogRecordType type, const storage::RID& rid, std::string_view tuple)
        : size_(HeaderSize() + sizeof(storage::RID) + tuple.size()),
          type_(type),
          txn_id_(txn_id),
          prev_lsn_(0),
          insert_rid_(rid),
          insert_tuple_(tuple) {}

    // Размер заголовка лога
    static constexpr uint32_t HeaderSize() { return 20; } // size(4) + type(4) + txn_id(8) + prev_lsn(4)

    // При восстановлении (Crash Recovery - фаза Redo/Undo) запись должна уметь
    // сериализоваться / десериализоваться из непрерывного char* буфера (io_uring).
    
    // В Production для этого используются zero-copy flatbuffers или банальный reinterpret_cast.

    uint32_t GetSize() const { return size_; }
    LogRecordType GetType() const { return type_; }
    TxnId GetTxnId() const { return txn_id_; }

private:
    uint32_t size_{0};                  // Длина всей записи лога
    LogRecordType type_{LogRecordType::INVALID}; // Тип операции
    TxnId txn_id_{0};                   // Идентификатор Тракзакции
    TypeEpoch prev_lsn_{0};             // LSN предыдущей записи этой транзакции (для Undo связаного списка)

    // Тело INSERT/UPDATE/DELETE
    storage::RID insert_rid_{};
    std::string insert_tuple_;          // Логический кортеж (будет сериализован)
};

} // namespace stingdb::transaction