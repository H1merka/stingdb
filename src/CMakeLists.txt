add_library(common_memory common/memory/arena_allocator.cpp)
add_library(storage_disk storage/disk/disk_manager.cpp)
add_library(storage_page storage/page/page.cpp)
add_library(storage_buffer_pool storage/buffer_pool/buffer_pool_manager.cpp)
add_library(storage_table storage/table/slotted_page.cpp)
add_library(storage_index storage/index/b_plus_tree.cpp)

# Header only for transaction layer in this iteration
add_library(transaction INTERFACE)

# Link everything together
add_library(stingdb
    $<TARGET_OBJECTS:common_memory>
    $<TARGET_OBJECTS:storage_disk>
    $<TARGET_OBJECTS:storage_page>
    $<TARGET_OBJECTS:storage_buffer_pool>
    $<TARGET_OBJECTS:storage_table>
    $<TARGET_OBJECTS:storage_index>
)
target_link_libraries(stingdb PUBLIC transaction)

target_link_libraries(stingdb PRIVATE ${URING_LIBRARIES} pthread)
target_include_directories(stingdb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
