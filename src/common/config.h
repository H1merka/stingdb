#pragma once

#include <cstdint>
#include <cstddef>

namespace stingdb::common {

// Стандартный размер страницы - 8KB для оптимального взаимодействия с NVMe и файловыми системами.
inline constexpr std::size_t PAGE_SIZE = 8192;

// Идентификатор страницы (uint32_t позволяет адресовать до 32TB данных при размере 8KB на страницу).
using PageId = uint32_t;

// Идентификатор невалидной страницы.
inline constexpr PageId INVALID_PAGE_ID = 0xFFFFFFFF;

using frame_id_t = int32_t;

} // namespace stingdb::common