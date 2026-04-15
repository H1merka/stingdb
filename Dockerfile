# ==========================================
# Stage 1: Build (Сборка)
# ==========================================
FROM ubuntu:24.04 AS builder

# Избегаем интерактивных диалогов при apt-get install
ENV DEBIAN_FRONTEND=noninteractive

# Устанавливаем строгие компиляторы и утилиты
RUN apt-get update && apt-get install -y \
    clang-18 \
    cmake \
    ninja-build \
    libboost-system-dev \
    libboost-thread-dev \
    liburing-dev \
    git \
    && rm -rf /var/lib/apt/lists/*

# Делаем clang-18 компилятором по умолчанию для CMake
ENV CC=clang-18
ENV CXX=clang++-18

# Копируем исходный код проекта
WORKDIR /build
COPY . /build/

# Конфигурируем и собираем в режиме Release (максимальная оптимизация O3)
RUN cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j$(nproc)

# ==========================================
# Stage 2: Runtime (Промышленный образ)
# ==========================================
FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

# Ставим только необходимые run-time зависимости (динамические библиотеки)
RUN apt-get update && apt-get install -y \
    libboost-system1.83.0 \
    libboost-thread1.83.0 \
    liburing2 \
    && rm -rf /var/lib/apt/lists/*

# Безопасность: Создаем непривилегированного пользователя stingdb (UID 999)    
RUN groupadd -g 999 stingdb && \
    useradd -r -u 999 -g stingdb stingdb

# Создаем директорию для базы данных и отдаем права нашему пользователю
RUN mkdir -p /var/lib/stingdb/data && \
    chown -R stingdb:stingdb /var/lib/stingdb

# Назначаем рабочую директорию, именно тут движок создаст 'stingdb_data.db'    
WORKDIR /var/lib/stingdb/data

# Забираем собранный бинарник из стадии builder
COPY --from=builder --chown=stingdb:stingdb /build/build/src/stingdb-server /usr/local/bin/stingdb-server

# Переключаемся на безопасного пользователя
USER stingdb

# Экспонируем порт TCP Реактора СУБД
EXPOSE 5432

# Команда по умолчанию для запуска контейнера
CMD ["stingdb-server"]