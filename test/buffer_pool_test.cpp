#include <gtest/gtest.h>
#include "storage/disk/disk_manager.h"
#include "storage/buffer_pool/buffer_pool_manager.h"
#include <cstdio>
#include <unistd.h>
#include <string>
#include <cstring>

/**
 * Базовое Интеграционное тестирование подсистемы Buffer Pool + Disk Manager
 * (io_uring и O_DIRECT I/O). 
 */
TEST(BufferPoolManagerTest, BasicFetchPage) {
    std::string test_db = "stingdb_integration_test.db";
    
    // Создаем контекст СУБД 
    {
        stingdb::storage::DiskManager disk_manager(test_db);
        // Небольшой буферный пул на 10 страниц 
        stingdb::storage::BufferPoolManager bpm(10, &disk_manager);

        // 1. Читаем страницу 0, пишем в нее
        stingdb::storage::Page* page0 = bpm.FetchPage(0);
        EXPECT_NE(page0, nullptr);
        EXPECT_EQ(page0->GetPageId(), 0);
        
        sprintf(page0->GetData(), "StingDB Test Data String");
        
        // 2. Отпускаем и помечаем изменения как "dirty"
        EXPECT_TRUE(bpm.UnpinPage(0, true));
        
        // 3. Форсируем сброс. Данные должны пройти через io_uring.
        EXPECT_TRUE(bpm.FlushPage(0));
    } // Деструктор очистит память

    // Проверяем, что данные дошли до жесткого диска 
    {
        stingdb::storage::DiskManager disk_manager2(test_db);
        stingdb::storage::BufferPoolManager bpm2(10, &disk_manager2);

        // Забираем сброшенную страницу 
        stingdb::storage::Page* page0_recovered = bpm2.FetchPage(0);
        EXPECT_NE(page0_recovered, nullptr);
        
        // Сравниваем данные 
        EXPECT_STREQ(page0_recovered->GetData(), "StingDB Test Data String");
        bpm2.UnpinPage(0, false);
    }
    
    // Очистка дискового пространства
    unlink(test_db.c_str());
}