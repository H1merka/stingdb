#pragma once

#include "execution/executor.h"
#include "execution/executor_context.h"
#include "catalog/table_info.h"
#include "storage/table/slotted_page.h"

#include <memory>
#include <string>

namespace stingdb::execution {

/**
 * Исполнитель вставки (Insert).
 * Принимает колоночные батчи от дочернего узла (например, ValuesExecutor)
 * и преобразует их в строковое представление для вставки в SlottedPage хранения.
 */
class InsertExecutor : public ExecutionOperator {
public:
    InsertExecutor(ExecutorContext* context, 
                   std::unique_ptr<ExecutionOperator> child, 
                   const std::string& table_name)
        : context_(context), child_(std::move(child)), table_name_(table_name) {}

    void Init() override {
        child_->Init();
        table_info_ = context_->GetCatalog()->GetTable(table_name_);
        if (!table_info_) {
            throw std::runtime_error("Table not found for INSERT: " + table_name_);
        }
    }

    bool Next(TupleBatch* batch_out) override {
        if (!table_info_) return false;

        TupleBatch input_batch;
        
        // В рамках MVP: обрабатываем только первый успешный батч вставок от ребенка.
        if (child_->Next(&input_batch)) {
            // Для каждой строки в батче, мы должны сериализовать данные в N-ary формат
            // согласно Schema (которую мы получили из Каталога).
            
            // Получаем страницу для записи (в production здесь ищется свободное место, FSM). 
            // В MVP: используем first_page_id или создаем новую страницу, если ее нет.
            auto* bpm = context_->GetBufferPool();
            common::PageId page_id = table_info_->GetFirstPageId();
            storage::Page* raw_page = nullptr;

            if (page_id == common::INVALID_PAGE_ID) {
                // Таблица пустая, выделяем ей первую страницу данных.
                raw_page = bpm->NewPage(&page_id);
                table_info_->UpdateFirstPageId(page_id);
                storage::SlottedPage new_slotted(raw_page);
                new_slotted.Init();
            } else {
                raw_page = bpm->FetchPage(page_id);
            }

            storage::SlottedPage target_page(raw_page);
            
            // В MVP - заглушка сериализации (сохраняем просто пустые байты размером со схемой).
            // Реальная сериализация должна обходить колонки в input_batch по row_idx и копировать байты в buffer.
            std::string serialized_tuple(table_info_->GetSchema().GetTupleSize(), '\0');

            storage::RID inserted_rid;
            size_t inserted_count = 0;
            
            // Цикл по строкам текущего батча
            for (size_t i = 0; i < input_batch.GetNumRows(); ++i) {
                if (target_page.InsertTuple(serialized_tuple, &inserted_rid)) {
                    inserted_count++;
                    
                    // Логирование вставки для обеспечения атомарности (UNDO/REDO)
                    transaction::Transaction* txn = context_->GetTransaction();
                    if (txn) {
                        transaction::LogRecord record(
                            txn->GetTxnId(), 
                            target_page.GetLSN(), 
                            transaction::LogRecordType::INSERT, 
                            inserted_rid, 
                            serialized_tuple
                        );
                        
                        transaction::TypeEpoch lsn = context_->GetTransactionManager()->GetLogManager()->AppendLogRecord(&record);
                        txn->AppendWriteSet(lsn);
                        target_page.SetLSN(lsn); // Обновляем LSN на странице, чтобы при краше восстановить консистентность
                    }

                } else {
                    // Обработка OOM (Out Of Space): выделяем новую страницу. В MVP опустим этот корнеркейс.
                    break;
                }
            }

            bpm->UnpinPage(page_id, true); // true - страница была модифицирована (грязная)

            // Insert Operator не возвращает данные (или возвращает число вставленных строк)
            return false; 
        }

        return false;
    }

private:
    ExecutorContext* context_;
    std::unique_ptr<ExecutionOperator> child_;
    std::string table_name_;
    catalog::TableInfo* table_info_{nullptr};
};

} // namespace stingdb::execution