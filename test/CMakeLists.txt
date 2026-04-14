cmake_minimum_required(VERSION 3.20)

set(CMAKE_CXX_STANDARD 20)

add_executable(stingdb-test buffer_pool_test.cpp)

# Подключение к библиотеке СУБД ядра и GTest фреймворку
target_link_libraries(stingdb-test PRIVATE stingdb GTest::gtest_main GTest::gtest pthread)
target_include_directories(stingdb-test PRIVATE ${CMAKE_SOURCE_DIR}/src ${CMAKE_SOURCE_DIR}/include)

# Регистрация бинарника в диспетчере CTest
add_test(NAME StingDB_IntegrationTest COMMAND stingdb-test)