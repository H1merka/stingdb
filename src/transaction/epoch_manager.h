#pragma once

#include <atomic>
#include <vector>
#include <cstdint>

namespace stingdb::transaction {

using TypeEpoch = uint64_t;
using TxnId = uint64_t;

/**
 * EpochManager: Высокопроизводительный механизм контроля версий времени (TSO)
 * 
 * В HighLoad системах взятие глобального std::atomic::fetch_add счетчика 
 * на старт/коммит каждой транзакции мгновенно вызывает cache line bouncing (contention)
 * между ядрами CPU (особенно на NUMA).
 * 
 * Решение: Время квантуется на "Эпохи" (Epochs). Каждая эпоха длится, например, 
 * 1-5 мс. Транзакции читают текущую эпоху через memory_order_acquire. 
 * Сборщик мусора (GC) для версий MVCC (Multi-Version Concurrency Control) 
 * очищает дельты только для тех эпох, из которых ушли все активные читатели.
 */
class EpochManager {
public:
    EpochManager() : global_epoch_(1), active_epochs_(MAX_EPOCHS, 0) {}

    // Отключение копирования/перемещения
    EpochManager(const EpochManager&) = delete;
    EpochManager& operator=(const EpochManager&) = delete;

    // Вход транзакции в систему. Регистрация читателя в текущей эпохе.
    TypeEpoch EnterEpoch() {
        TypeEpoch current = global_epoch_.load(std::memory_order_acquire);
        
        // В продакшене используем thread_local для локального счетчика 
        // и периодически сливаем в глобальный массив для GC.
        // Здесь: инкрементируем количество читателей для текущей эпохи.
        active_epochs_[current % MAX_EPOCHS].fetch_add(1, std::memory_order_release);
        return current;
    }

    // Выход транзакции из системы
    void ExitEpoch(TypeEpoch epoch) {
        active_epochs_[epoch % MAX_EPOCHS].fetch_sub(1, std::memory_order_release);
    }

    // Сдвиг глобальной эпохи. Вызывается фоновым потоком каждые N миллисекунд.
    void AdvanceEpoch() {
        global_epoch_.fetch_add(1, std::memory_order_release);
    }

    // Вычисление "самой старой" эпохи, в которой еще есть читатели (для Garbage Collection - очистки UNDO логов)
    TypeEpoch GetOldestActiveEpoch() {
        TypeEpoch current = global_epoch_.load(std::memory_order_acquire);
        for (TypeEpoch e = current; e > 0; --e) {
            if (active_epochs_[e % MAX_EPOCHS].load(std::memory_order_acquire) > 0) {
                // Если мы дошли до прошлой эпохи и там есть транзакции, 
                // самая старая активная эпоха будет предшествующей этому скану,
                // но для упрощения здесь возвращаем минимум (e) среди активных
                return e;
            }
        }
        return current; // Если никто не читает, можно удалять все до текущей эпохи.
    }

private:
    static constexpr std::size_t MAX_EPOCHS = 1024; // Циклический буфер эпох
    
    // Выравнивание для предотвращения False Sharing между ядрами L1/L2
    alignas(64) std::atomic<TypeEpoch> global_epoch_;
    
    // Количество потоков-читателей внутри конкретной эпохи
    // Выравниваем каждый атомик до 64 байт (строка кэша)
    struct AlignedCounter {
        alignas(64) std::atomic<int32_t> count{0};
    };
    std::vector<AlignedCounter> active_epochs_;
};

} // namespace stingdb::transaction