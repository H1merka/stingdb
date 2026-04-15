#pragma once

#include "execution/executor.h"
#include "execution/executor_context.h"
#include "storage/table/slotted_page.h"

namespace stingdb::execution {

/**
 * Исполнитель последовательного сканирования (Sequential Scan).
 * Читает данные со страниц табличной кучи (Heap) и конвертирует 
 * N-ary кортежи в колоночные батчи (TupleBatch).
 */
class SeqScanExecutor : public ExecutionOperator {
public:
    SeqScanExecutor(ExecutorContext* context, const std::string& table_name)
        : context_(context), table_name_(table_name) {}

    void Init() override {
        table_info_ = context_->GetCatalog()->GetTable(table_name_);
        if (!table_info_) {
            throw std::runtime_error("Table not found: " + table_name_);
        }
        current_page_id_ = table_info_->GetFirstPageId();
        current_slot_ = 0;
    }

    bool Next(TupleBatch* batch_out) override {
        if (!table_info_ || current_page_id_ == common::INVALID_PAGE_ID) {
            return false;
        }

        // Подготовим батч согласно схеме (в упрощенном MVP варианте мы читаем 1 кортеж за раз и возвращаем его как батч из 1 строки).
        // В продакшене: накапливать до TupleBatch::DEFAULT_BATCH_SIZE.
        auto* bpm = context_->GetBufferPool();
        
        storage::Page* raw_page = bpm->FetchPage(current_page_id_);
        if (!raw_page) {
            return false;
        }

        storage::SlottedPage slotted_page(raw_page);
        
        if (current_slot_ >= slotted_page.GetTupleCount()) {
            // На текущей странице кортежи закончились.
            // В MVP: у SlottedPage пока нет next_page_id (linked list of pages),
            // поэтому мы просто заканчиваем сканирование на 1 странице.
            bpm->UnpinPage(current_page_id_, false);
            return false; 
        }

        storage::RID rid{current_page_id_, current_slot_};
        std::string tuple_data;
        if (slotted_page.GetTuple(rid, &tuple_data)) {
            // MVCC: В будущем здесь будет проверка версии кортежа.
            // Если транзакция видит эту версию (tuple.commit_ts <= txn->read_epoch_),
            // мы включаем ее в батч.
            // Если версия 'из будущего' или создана 'незакомитченной' чужой транзакцией (а мы не READ_UNCOMMITTED),
            // мы либо пропускаем ее, либо ищем предыдущую версию в UNDO логах (Version Chain).

            // TODO: Распарсить tuple_data (бинарный кортеж) согласно Schema и записать поля в TupleBatch.
            // Пока мы только извлекаем сырые данные для интеграции.
        }

        current_slot_++;
        bpm->UnpinPage(current_page_id_, false);

        // В MVP мы возвращаем true, чтобы симулировать успешный fetch (пустышка для батча)
        return true; 
    }

private:
   ExecutorContext* context_;
   std::string table_name_;
   catalog::TableInfo* table_info_{nullptr};
   common::PageId current_page_id_{common::INVALID_PAGE_ID};
   uint32_t current_slot_{0};
};

} // namespace stingdb::execution