#pragma once

#include "storage/page/page.h"
#include <cstdint>
#include <tuple>
#include <string_view>

namespace stingdb::storage {

// Идентификатор кортежа (строки) в таблице - Record ID
struct RID {
    common::PageId page_id{common::INVALID_PAGE_ID};
    uint32_t slot_num{0};

    bool operator==(const RID& rhs) const {
        return page_id == rhs.page_id && slot_num == rhs.slot_num;
    }
};

/**
 * SlottedPage: Абстракция над сырой страницей памяти для эффективного размещения 
 * записей (кортежей) переменной длины.
 * 
 * Структура страницы:
 * ---------------------------------------------------------
 * | Header | Slot Array -> | ... Free Space ... | <- Tuples |
 * ---------------------------------------------------------
 * 
 * Header состоит из:
 * LSN (Log Sequence Number) - 8 байт (подготовка к Итерации 3)
 * Tuple Count - 2 байта
 * Free Space Pointer - 2 байта
 */
class SlottedPage {
public:
    static constexpr std::size_t LSN_OFFSET = 0;
    static constexpr std::size_t TUPLE_COUNT_OFFSET = 8;
    static constexpr std::size_t FREE_SPACE_OFFSET = 10;
    static constexpr std::size_t HEADER_SIZE = 12;

    static constexpr std::size_t SLOT_SIZE = 4; // 2 байта смещение, 2 байта размер кортежа

    explicit SlottedPage(Page* page) : page_(page) {}

    // Инициализация новой страницы
    void Init();

    // Вставка кортежа. Возвращает true и устанавливает rid, если достаточно места
    bool InsertTuple(std::string_view tuple_data, RID* rid);

    // Удаление кортежа (помечаем слот как мертвый, физическое удаление откладываем до вакуумирования)
    bool MarkDelete(const RID& rid);

    // Извлечение кортежа
    bool GetTuple(const RID& rid, std::string* tuple_data) const;
    
    // Получение LSN для WAL (будет использоваться в Итерации 3)
    [[nodiscard]] uint64_t GetLSN() const;
    void SetLSN(uint64_t lsn);

    [[nodiscard]] uint16_t GetTupleCount() const;

    // Доступ к базовой странице для операций Buffer Pool'а
    [[nodiscard]] Page* GetRawPage() const { return page_; }

private:
    void SetTupleCount(uint16_t tuple_count);

    [[nodiscard]] uint16_t GetFreeSpacePointer() const;
    void SetFreeSpacePointer(uint16_t free_space_pointer);

    Page* page_;
};

} // namespace stingdb::storage
