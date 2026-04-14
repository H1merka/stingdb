cmake_minimum_required(VERSION 3.20)
project(StingDB VERSION 0.1.0 LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS False)

# Строгие флаги компилятора для production-ready продукта
set(CMAKE_CXX_FLAGS "-Wall -Wextra -Wpedantic -Wconversion -Werror -Wno-unused-parameter -O3 -march=native")

# Подключение liburing (важно для production I/O)
find_package(PkgConfig REQUIRED)
pkg_check_modules(URING REQUIRED liburing)

# Векторизация и SIMD
# Библиотека xsimd используется для кросс-платформенной (AVX/SSE/NEON) поддержки SIMD
find_package(xsimd REQUIRED)

# Сетевой слой и Реактор
# Подключение Boost.Asio для асинхронной обработки тысяч TCP соединений
find_package(Boost REQUIRED)

include_directories(include src)

add_subdirectory(src)
