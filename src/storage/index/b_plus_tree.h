#pragma once

#include "storage/buffer_pool/buffer_pool_manager.h"
#include "storage/page/page.h"
#include "storage/table/slotted_page.h"

namespace stingdb::storage {

// Типы узлов для B+Tree
enum class BPlusTreeNodeType { INVALID = 0, INTERNAL = 1, LEAF = 2 };

// В реальной production-системе B+Tree должно быть шаблонизировано по KeyType/ValueType.
// Здесь мы создаем фундамент архитектуры и структуру дерева. В ключевом узле храним 
// просто 64-битные ключи для упрощения логики Итерации 2. Значения - это RID (указатели на Slotted Page)
// для листовых узлов или PageId для внутренних.
class BPlusTreeNode {
public:
    static constexpr std::size_t HEADER_SIZE = 16; // Type (4), Size (4), MaxSize (4), Parent (4)
    
    // Инициализация вершины дерева в страницы
    void Init(BPlusTreeNodeType type, uint32_t max_size);
    
    [[nodiscard]] BPlusTreeNodeType GetType() const;
    [[nodiscard]] uint32_t GetSize() const;
    void SetSize(uint32_t size);
    void IncreaseSize(int by);
    
    [[nodiscard]] uint32_t GetMaxSize() const;
    [[nodiscard]] common::PageId GetParentPageId() const;
    void SetParentPageId(common::PageId parent);
    [[nodiscard]] bool IsLeafPage() const;
    [[nodiscard]] bool IsRootPage() const;

private:
    alignas(16) char data_[common::PAGE_SIZE]; // Алиас для размещения внутри Page
};

/**
 * Concurrent B+Tree Index with Optimistic Lock Coupling Interface.
 * Архитектура оптимизирована для снижения Latch Contention (блокировок) 
 * с помощью техники кратковременного R-Lock протокола на обходе (traverse)
 * и апгрейда до W-Lock только на Leaf узлах.
 */
class BPlusTree {
public:
    explicit BPlusTree(BufferPoolManager* bpm, common::PageId root_page_id = common::INVALID_PAGE_ID);

    // Интерфейс для оптимистичной вставки в индекс
    bool Insert(int64_t key, const RID& rid);

    // Точечный поиск
    bool FindValue(int64_t key, RID* rid_out);

    // В Итерации 3 добавится удаление и Range Scans.
    // bool Remove(int64_t key);

private:
    // Контекст для техники Lock Coupling: храним текущую 'latched' страницу.
    // В оптимистическом варианте мы проверяем версию узла (Version/Counter), 
    // но здесь реализован Crabbing Latch (R-Lock на спуск, W-Lock на листьях).
    struct SearchContext {
        Page* page{nullptr};
        bool is_write{false};
        
        ~SearchContext() {
            if (page) {
                // Если мы заблокировали страницу - освобождаем ее и unpin
                if (is_write) page->WUnlock();
                else page->RUnlock();
                // BufferPoolManager->Unpin(page->GetPageId(), is_write);
            }
        }
    };

    BufferPoolManager* bpm_;
    common::PageId root_page_id_;

    // Вспомогательные методы
    Page* FindLeafPage(int64_t key, SearchContext* context);
    void InsertIntoLeaf(Page* leaf_page, int64_t key, const RID& rid);
};

} // namespace stingdb::storage