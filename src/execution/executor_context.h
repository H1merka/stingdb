#pragma once

#include "catalog/catalog_manager.h"
#include "storage/buffer_pool/buffer_pool_manager.h"
#include "transaction/transaction.h"
#include "transaction/transaction_manager.h"

namespace stingdb::execution {

/**
 * Контекст исполнения запроса.
 * Хранит глобальные и транзакционные ссылки, необходимые операторам:
 * - Доступ к метаданным (Каталогу)
 * - Доступ к страницам на диске (Buffer Pool Manager)
 * - Контекст текущей выполняемой транзакции
 */
class ExecutorContext {
public:
    ExecutorContext(catalog::CatalogManager* catalog,
                    storage::BufferPoolManager* buffer_pool,
                    transaction::Transaction* txn,
                    transaction::TransactionManager* txn_mgr)
        : catalog_(catalog), buffer_pool_(buffer_pool),
          txn_(txn), txn_mgr_(txn_mgr) {}

    [[nodiscard]] catalog::CatalogManager* GetCatalog() const { return catalog_; }
    [[nodiscard]] storage::BufferPoolManager* GetBufferPool() const { return buffer_pool_; }
    [[nodiscard]] transaction::Transaction* GetTransaction() const { return txn_; }
    [[nodiscard]] transaction::TransactionManager* GetTransactionManager() const { return txn_mgr_; }

private:
    catalog::CatalogManager* catalog_;
    storage::BufferPoolManager* buffer_pool_;
    transaction::Transaction* txn_;
    transaction::TransactionManager* txn_mgr_;
};

} // namespace stingdb::execution