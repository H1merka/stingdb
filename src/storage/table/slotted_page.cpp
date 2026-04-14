#include "storage/table/slotted_page.h"
#include <cstring>
#include <memory>

namespace stingdb::storage {

void SlottedPage::Init() {
    page_->ResetMemory();
    SetLSN(0);
    SetTupleCount(0);
    // Свободное пространство начинается с конца страницы
    SetFreeSpacePointer(static_cast<uint16_t>(common::PAGE_SIZE));
}

bool SlottedPage::InsertTuple(std::string_view tuple_data, RID* rid) {
    if (tuple_data.size() > static_cast<std::size_t>(UINT16_MAX)) {
        return false; // Слишком большая запись
    }

    uint16_t tuple_size = static_cast<uint16_t>(tuple_data.size());
    uint16_t tuple_count = GetTupleCount();
    uint16_t free_space = GetFreeSpacePointer();

    // Вычисление необходимого пространства (данные кортежа + слот)
    uint16_t required_space = static_cast<uint16_t>(tuple_size + SLOT_SIZE);
    
    // Проверка доступного непрерывного места (между массивом слотов и свободным 공간ом)
    uint16_t used_space_header = static_cast<uint16_t>(HEADER_SIZE + (tuple_count * SLOT_SIZE));
    if (free_space - used_space_header < required_space) {
        return false; // Недостаточно места (потребуется компактификация или другая страница)
    }

    // Аллокация места с конца
    free_space -= tuple_size;
    SetFreeSpacePointer(free_space);

    // Копирование данных кортежа (tuple data)
    memcpy(page_->GetData() + free_space, tuple_data.data(), tuple_size);

    // Запись метаданных в слот (смещение + размер)
    uint16_t slot_offset = static_cast<uint16_t>(HEADER_SIZE + (tuple_count * SLOT_SIZE));
    memcpy(page_->GetData() + slot_offset, &free_space, sizeof(uint16_t));
    memcpy(page_->GetData() + slot_offset + sizeof(uint16_t), &tuple_size, sizeof(uint16_t));

    // Установка RID и инкремент счетчика
    rid->page_id = page_->GetPageId();
    rid->slot_num = tuple_count;

    SetTupleCount(tuple_count + 1);
    page_->SetDirty(true);
    return true;
}

bool SlottedPage::GetTuple(const RID& rid, std::string* tuple_data) const {
    uint16_t tuple_count = GetTupleCount();
    if (rid.slot_num >= tuple_count) {
        return false;
    }

    uint16_t slot_offset = static_cast<uint16_t>(HEADER_SIZE + (rid.slot_num * SLOT_SIZE));
    uint16_t tuple_offset = 0;
    uint16_t tuple_size = 0;

    memcpy(&tuple_offset, page_->GetData() + slot_offset, sizeof(uint16_t));
    memcpy(&tuple_size, page_->GetData() + slot_offset + sizeof(uint16_t), sizeof(uint16_t));

    // Размер = 0 может сигнализировать о мертвой записи (Tombstone_
    if (tuple_size == 0) {
        return false;
    }

    tuple_data->assign(page_->GetData() + tuple_offset, tuple_size);
    return true;
}

bool SlottedPage::MarkDelete(const RID& rid) {
    uint16_t tuple_count = GetTupleCount();
    if (rid.slot_num >= tuple_count) {
        return false;
    }
    
    // В текущей итерации просто обнуляем размер в слоте (Tombstone). 
    // Настоящая сборка мусора (vacuum) переупакует страницу при нехватке места.
    uint16_t slot_offset = static_cast<uint16_t>(HEADER_SIZE + (rid.slot_num * SLOT_SIZE));
    uint16_t zero_size = 0;
    memcpy(page_->GetData() + slot_offset + sizeof(uint16_t), &zero_size, sizeof(uint16_t));
    
    page_->SetDirty(true);
    return true;
}

uint64_t SlottedPage::GetLSN() const {
    uint64_t lsn = 0;
    memcpy(&lsn, page_->GetData() + LSN_OFFSET, sizeof(uint64_t));
    return lsn;
}

void SlottedPage::SetLSN(uint64_t lsn) {
    memcpy(page_->GetData() + LSN_OFFSET, &lsn, sizeof(uint64_t));
}

uint16_t SlottedPage::GetTupleCount() const {
    uint16_t count = 0;
    memcpy(&count, page_->GetData() + TUPLE_COUNT_OFFSET, sizeof(uint16_t));
    return count;
}

void SlottedPage::SetTupleCount(uint16_t count) {
    memcpy(page_->GetData() + TUPLE_COUNT_OFFSET, &count, sizeof(uint16_t));
}

uint16_t SlottedPage::GetFreeSpacePointer() const {
    uint16_t pointer = 0;
    memcpy(&pointer, page_->GetData() + FREE_SPACE_OFFSET, sizeof(uint16_t));
    return pointer;
}

void SlottedPage::SetFreeSpacePointer(uint16_t pointer) {
    memcpy(page_->GetData() + FREE_SPACE_OFFSET, &pointer, sizeof(uint16_t));
}

} // namespace stingdb::storage
