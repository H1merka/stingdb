# StingDB

StingDB is a high-performance, experimental relational database engine built from scratch in C++20. Designed for high concurrency and throughput, it leverages modern Linux asynchronous I/O and vectorized execution models.

## Key Features

* **Modern C++20**: Built with strict Clang 18 compiler flags (`-Werror`, `-Wpedantic`, `-Wconversion`) for optimal safety and performance.
* **Asynchronous Networking**: Event-driven TCP Reactor using `Boost.Asio` (`epoll`-based) under a scalable thread pool architecture.
* **Advanced Storage Engine**: On-disk storage featuring a `BufferPoolManager`, Slotted Page data layout, and asynchronous disk operations via `io_uring`.
* **Vectorized Execution**: Processes data in `TupleBatch`es (integrated with `xsimd` to leverage AVX/SIMD instructions).
* **Transaction Management**: Foundational support for MVCC (Multi-Version Concurrency Control) and physiological Write-Ahead Logging (WAL).

## Getting Started

### Prerequisites
* Linux environment (Ubuntu 24.04 recommended)
* Clang 18, CMake, Ninja-build
* Boost 1.83+ (`libboost-system-dev`, `libboost-thread-dev`)
* liburing (`liburing-dev`)

### Build
```bash
# Generate build files
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile the project
ninja -C build
```

### Run
```bash
./build/stingdb-server
```

## Usage (MVP)

StingDB currently utilizes a Raw Text Protocol for its minimum viable product. You can interact with the running server using standard TCP clients like `netcat` (`nc`) or `telnet`:

```bash
nc 127.0.0.1 5432
```

**Supported commands:**
* `CREATE TABLE` - Initializes the schema for a test table.
* `INSERT` - Executes the InsertExecutor, adding a new record to the disk pages.
* `SELECT` - Executes the SeqScanExecutor to fetch data from the disk pages.
* `BEGIN` - Starts a new Snapshot Isolation transaction.

## Deployment

The repository includes Infrastructure as Code (IaC) definitions for containerized deployment:
* **Docker / Docker Compose**: Run `docker-compose up -d --build` for a fully tuned local container deployment.
* **Kubernetes**: `StatefulSet` and Headless Service manifests suitable for Stateful deployments are available in the `k8s/` directory.

## License

Please refer to the `LICENSE` file for distribution terms.
