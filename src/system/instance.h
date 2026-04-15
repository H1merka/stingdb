#pragma once

#include "storage/buffer_pool/buffer_pool_manager.h"
#include "catalog/catalog_manager.h"
#include "transaction/transaction_manager.h"
#include "transaction/log_manager.h"

namespace stingdb::system {

/**
 * StingInstance: Глобальный контейнер СУБД.
 * Владеет всеми singleton-менеджерами и обеспечивает безопасный
 * к ним доступ из различных рабочих потоков (Event-loop Thread Pool).
 */
class StingInstance {
public:
    explicit StingInstance(storage::BufferPoolManager* bpm)
        : bpm_(bpm),
          log_manager_(),
          txn_manager_(&log_manager_),
          catalog_manager_() {}

    [[nodiscard]] storage::BufferPoolManager* GetBufferPool() { return bpm_; }
    [[nodiscard]] transaction::LogManager* GetLogManager() { return &log_manager_; }
    [[nodiscard]] transaction::TransactionManager* GetTransactionManager() { return &txn_manager_; }
    [[nodiscard]] catalog::CatalogManager* GetCatalogManager() { return &catalog_manager_; }

private:
   storage::BufferPoolManager* bpm_;
   transaction::LogManager log_manager_;
   transaction::TransactionManager txn_manager_;
   catalog::CatalogManager catalog_manager_;
};

} // namespace stingdb::system