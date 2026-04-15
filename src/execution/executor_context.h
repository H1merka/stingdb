#pragma once

#include "catalog/catalog_manager.h"
#include "storage/buffer_pool/buffer_pool_manager.h"

namespace stingdb::execution {

/**
 * Контекст исполнения запроса.
 * Хранит глобальные и транзакционные ссылки, необходимые операторам:
 * - Доступ к метаданным (Каталогу)
 * - Доступ к страницам на диске (Buffer Pool Manager)
 * В будущих итерациях сюда добавится TransactionContext.
 */
class ExecutorContext {
public:
    ExecutorContext(catalog::CatalogManager* catalog,
                    storage::BufferPoolManager* buffer_pool)
        : catalog_(catalog), buffer_pool_(buffer_pool) {}

    [[nodiscard]] catalog::CatalogManager* GetCatalog() const { return catalog_; }
    [[nodiscard]] storage::BufferPoolManager* GetBufferPool() const { return buffer_pool_; }

private:
    catalog::CatalogManager* catalog_;
    storage::BufferPoolManager* buffer_pool_;
};

} // namespace stingdb::execution