add_library(common_memory common/memory/arena_allocator.cpp)
add_library(storage_disk storage/disk/disk_manager.cpp)
add_library(storage_page storage/page/page.cpp)
add_library(storage_buffer_pool storage/buffer_pool/buffer_pool_manager.cpp)

# Link everything together
add_library(stingdb
    $<TARGET_OBJECTS:common_memory>
    $<TARGET_OBJECTS:storage_disk>
    $<TARGET_OBJECTS:storage_page>
    $<TARGET_OBJECTS:storage_buffer_pool>
)

target_link_libraries(stingdb PRIVATE ${URING_LIBRARIES} pthread)
target_include_directories(stingdb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
