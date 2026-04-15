#pragma once

#include "catalog/table_info.h"
#include <unordered_map>
#include <string>
#include <mutex>
#include <memory>
#include <atomic>
#include <stdexcept>

namespace stingdb::catalog {

class CatalogManager {
public:
    CatalogManager() = default;

    // В будущем тут должен быть StorageManager/BufferManager для персистенции. Пока In-Memory.
    TableInfo* CreateTable(const std::string& table_name, const Schema& schema, common::PageId first_page) {
        std::lock_guard<std::mutex> lock(catalog_mutex_);

        if (table_names_.contains(table_name)) {
            throw std::runtime_error("Table already exists: " + table_name);
        }

        TableId new_id = next_table_id_++;
        auto table_info = std::make_unique<TableInfo>(new_id, table_name, schema, first_page);
        TableInfo* info_ptr = table_info.get();
        
        tables_.emplace(new_id, std::move(table_info));
        table_names_[table_name] = new_id;

        return info_ptr;
    }

    TableInfo* GetTable(const std::string& table_name) {
        std::lock_guard<std::mutex> lock(catalog_mutex_);
        if (auto it = table_names_.find(table_name); it != table_names_.end()) {
            return tables_.at(it->second).get();
        }
        return nullptr;
    }

    TableInfo* GetTable(TableId table_id) {
         std::lock_guard<std::mutex> lock(catalog_mutex_);
         if (auto it = tables_.find(table_id); it != tables_.end()) {
             return it->second.get();
         }
         return nullptr;
    }

private:
   std::atomic<TableId> next_table_id_{0};
   std::unordered_map<TableId, std::unique_ptr<TableInfo>> tables_;
   std::unordered_map<std::string, TableId> table_names_;
   std::mutex catalog_mutex_;
};

} // namespace stingdb::catalog