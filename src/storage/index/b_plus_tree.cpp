#include "storage/index/b_plus_tree.h"
#include <cstring>
#include <iostream>

namespace stingdb::storage {

void BPlusTreeNode::Init(BPlusTreeNodeType type, uint32_t max_size) {
    uint32_t type_val = static_cast<uint32_t>(type);
    memcpy(data_, &type_val, sizeof(uint32_t));
    uint32_t size = 0;
    memcpy(data_ + sizeof(uint32_t), &size, sizeof(uint32_t));
    memcpy(data_ + 2 * sizeof(uint32_t), &max_size, sizeof(uint32_t));
    uint32_t parent = common::INVALID_PAGE_ID;
    memcpy(data_ + 3 * sizeof(uint32_t), &parent, sizeof(uint32_t));
}

BPlusTreeNodeType BPlusTreeNode::GetType() const {
    uint32_t t;
    memcpy(&t, data_, sizeof(uint32_t));
    return static_cast<BPlusTreeNodeType>(t);
}

uint32_t BPlusTreeNode::GetSize() const {
    uint32_t s;
    memcpy(&s, data_ + sizeof(uint32_t), sizeof(uint32_t));
    return s;
}

void BPlusTreeNode::SetSize(uint32_t size) {
    memcpy(data_ + sizeof(uint32_t), &size, sizeof(uint32_t));
}

void BPlusTreeNode::IncreaseSize(int by) {
    SetSize(GetSize() + static_cast<uint32_t>(by));
}

uint32_t BPlusTreeNode::GetMaxSize() const {
    uint32_t m;
    memcpy(&m, data_ + 2 * sizeof(uint32_t), sizeof(uint32_t));
    return m;
}

common::PageId BPlusTreeNode::GetParentPageId() const {
    uint32_t p;
    memcpy(&p, data_ + 3 * sizeof(uint32_t), sizeof(uint32_t));
    return p;
}

void BPlusTreeNode::SetParentPageId(common::PageId parent) {
    memcpy(data_ + 3 * sizeof(uint32_t), &parent, sizeof(uint32_t));
}

bool BPlusTreeNode::IsLeafPage() const {
    return GetType() == BPlusTreeNodeType::LEAF;
}

bool BPlusTreeNode::IsRootPage() const {
    return GetParentPageId() == common::INVALID_PAGE_ID;
}

BPlusTree::BPlusTree(BufferPoolManager* bpm, common::PageId root_page_id) 
    : bpm_(bpm), root_page_id_(root_page_id) {}

Page* BPlusTree::FindLeafPage(int64_t key, SearchContext* context) {
    if (root_page_id_ == common::INVALID_PAGE_ID) {
        return nullptr;
    }

    // Для production lock coupling'а: R-Lock на корню
    Page* current = bpm_->FetchPage(root_page_id_);
    current->RLock();

    BPlusTreeNode* node = reinterpret_cast<BPlusTreeNode*>(current->GetData());
    
    // Спуск по внутренним узлам: Crabbing
    while (!node->IsLeafPage()) {
        // Логика бинарного поиска внутри узла (Binary Search in node)
        // В этой итерации мы опускаем детали алгоритма для внутренних узлов
        common::PageId next_page_id = common::INVALID_PAGE_ID; // Будет вычислен бинарным поиском

        Page* next_page = bpm_->FetchPage(next_page_id);
        next_page->RLock();

        // Освобождение предка (Lock release)
        current->RUnlock();
        bpm_->UnpinPage(current->GetPageId(), false);

        current = next_page;
        node = reinterpret_cast<BPlusTreeNode*>(current->GetData());
    }

    // Если цель была 'чтение', возвращаем узел залоченным (RLock).
    // Если цель была 'запись', в lock coupling перед взятием RLock следовало бы 
    // проверить IsSafe(), и если нет - откатиться и брать WLock с корня.
    // Контекст сохранит локап (для автоматического release в RAII).
    
    context->page = current;
    context->is_write = false; // Для find
    
    return current;
}

bool BPlusTree::Insert(int64_t key, const RID& rid) {
    if (root_page_id_ == common::INVALID_PAGE_ID) {
        // Корень пуст. Создаем новую страницу.
        // Page* new_root = bpm_->NewPage(&root_page_id_);
        // InitLeaf(new_root);
        // ... (вставка через WLock) ...
        return true;
    }

    SearchContext ctx;
    // Оптимистичный обход дерева "без" блокировок по всему пути
    // (Используем OLC - Optimistic Lock Coupling. Для упрощения здесь Lock Crabbing)
    Page* leaf = FindLeafPage(key, &ctx);

    if (leaf) {
        // Обновляем лок с R (чтение) на W (запись) или полностью перезабираем с корня,
        // если вставка спровоцирует сплит страницы (split).
        // InsertIntoLeaf(leaf, key, rid);
        return true;
    }

    return false;
}

bool BPlusTree::FindValue(int64_t key, RID* rid_out) {
    SearchContext ctx;
    Page* leaf = FindLeafPage(key, &ctx);

    if (leaf) {
        [[maybe_unused]] BPlusTreeNode* node = reinterpret_cast<BPlusTreeNode*>(leaf->GetData());
        // Бинарный поиск O(log N) внутри leaf_page
        // memcpy(...) получение значения из слота
        return true;
    }

    return false;
}

} // namespace stingdb::storage