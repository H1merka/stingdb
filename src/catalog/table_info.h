#pragma once

#include "catalog/schema.h"
#include "common/config.h"
#include <string>

namespace stingdb::catalog {

using TableId = uint32_t;

/**
 * Объект метаданных таблицы.
 * Хранит логичное Имя, структуру данных (Schema) 
 * и указатель на первый физический блок памяти - корневую страницу (B+Tree Root или Head SlottedPage).
 */
class TableInfo {
public:
    TableInfo(TableId id, std::string name, Schema schema, common::PageId first_page_id)
        : id_(id), name_(std::move(name)), schema_(std::move(schema)), first_page_id_(first_page_id) {}

    [[nodiscard]] TableId GetId() const { return id_; }
    [[nodiscard]] const std::string& GetName() const { return name_; }
    [[nodiscard]] const Schema& GetSchema() const { return schema_; }
    [[nodiscard]] common::PageId GetFirstPageId() const { return first_page_id_; }

    void UpdateFirstPageId(common::PageId root) { first_page_id_ = root; }

private:
   TableId id_;
   std::string name_;
   Schema schema_;
   common::PageId first_page_id_; 
};

} // namespace stingdb::catalog