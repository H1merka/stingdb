add_library(common_memory common/memory/arena_allocator.cpp)
add_library(storage_disk storage/disk/disk_manager.cpp)
add_library(storage_page storage/page/page.cpp)
add_library(storage_buffer_pool storage/buffer_pool/buffer_pool_manager.cpp)
add_library(storage_table storage/table/slotted_page.cpp)
add_library(storage_index storage/index/b_plus_tree.cpp)

# Header only for transaction layer in this iteration
add_library(transaction INTERFACE)

# Header only abstract vectorized execution
add_library(execution INTERFACE)

# Header only SQL parser and Cost-Based Optimizer (CBO) iteration
add_library(sql_planner INTERFACE)

if(TARGET xsimd::xsimd)
    target_link_libraries(execution INTERFACE xsimd::xsimd)
elseif(TARGET xsimd)
    target_link_libraries(execution INTERFACE xsimd)
else()
    target_include_directories(execution INTERFACE ${xsimd_INCLUDE_DIR} ${xsimd_INCLUDE_DIRS})
endif()

# Link everything together
add_library(stingdb
    $<TARGET_OBJECTS:common_memory>
    $<TARGET_OBJECTS:storage_disk>
    $<TARGET_OBJECTS:storage_page>
    $<TARGET_OBJECTS:storage_buffer_pool>
    $<TARGET_OBJECTS:storage_table>
    $<TARGET_OBJECTS:storage_index>
)
target_link_libraries(stingdb PUBLIC transaction execution sql_planner ${URING_LIBRARIES} pthread)

target_link_libraries(stingdb PRIVATE ${URING_LIBRARIES} pthread)
target_include_directories(stingdb PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
