#pragma once

#include <atomic>
#include <vector>
#include <cstdint>
#include <array>

namespace stingdb::transaction {

using TypeEpoch = uint64_t;
using TxnId = uint64_t;

/**
 * EpochManager: Высокопроизводительный механизм контроля версий времени (TSO)
 */
class EpochManager {
public:
    EpochManager() : global_epoch_(1) {}

    EpochManager(const EpochManager&) = delete;
    EpochManager& operator=(const EpochManager&) = delete;

    TypeEpoch EnterEpoch() {
        TypeEpoch current = global_epoch_.load(std::memory_order_acquire);
        active_epochs_[current % MAX_EPOCHS].count.fetch_add(1, std::memory_order_release);
        return current;
    }

    void ExitEpoch(TypeEpoch epoch) {
        active_epochs_[epoch % MAX_EPOCHS].count.fetch_sub(1, std::memory_order_release);
    }

    void AdvanceEpoch() {
        global_epoch_.fetch_add(1, std::memory_order_release);
    }

    TypeEpoch GetOldestActiveEpoch() {
        TypeEpoch current = global_epoch_.load(std::memory_order_acquire);
        for (TypeEpoch e = current; e > 0; --e) {
            if (active_epochs_[e % MAX_EPOCHS].count.load(std::memory_order_acquire) > 0) {
                return e;
            }
        }
        return current; 
    }

private:
    static constexpr std::size_t MAX_EPOCHS = 1024;

    alignas(64) std::atomic<TypeEpoch> global_epoch_;

    struct AlignedCounter {
        alignas(64) std::atomic<int32_t> count{0};
    };
    std::array<AlignedCounter, MAX_EPOCHS> active_epochs_;
};

} // namespace stingdb::transaction
