#pragma once

#include "common/config.h"
#include "storage/table/slotted_page.h"
#include "transaction/epoch_manager.h"
#include <vector>
#include <string>

namespace stingdb::transaction {

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

class LogRecord {
public:
    LogRecord() = default;

    LogRecord(TxnId txn_id, TypeEpoch lsn, LogRecordType type)
        : size_(HeaderSize()),
          type_(type),
          txn_id_(txn_id),
          prev_lsn_(lsn) {}

    LogRecord(TxnId txn_id, TypeEpoch lsn, LogRecordType type, const storage::RID& rid, std::string_view tuple)
        : size_(static_cast<uint32_t>(HeaderSize() + sizeof(storage::RID) + tuple.size())),
          type_(type),
          txn_id_(txn_id),
          prev_lsn_(lsn),
          insert_rid_(rid),
          insert_tuple_(tuple) {}

    static constexpr uint32_t HeaderSize() { return 20; } 

    uint32_t GetSize() const { return size_; }
    LogRecordType GetType() const { return type_; }
    TxnId GetTxnId() const { return txn_id_; }
    TypeEpoch GetPrevLSN() const { return prev_lsn_; }

private:
    uint32_t size_{0};                  
    LogRecordType type_{LogRecordType::INVALID}; 
    TxnId txn_id_{0};                   
    TypeEpoch prev_lsn_{0};             

    storage::RID insert_rid_{};
    std::string insert_tuple_;          
};

} // namespace stingdb::transaction
