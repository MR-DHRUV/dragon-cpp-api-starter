# C++ REST API Backend Server

An asynchronous REST API backend built with modern **C++20**, **Drogon**, and **RapidJSON**, connected to **PostgreSQL** in a Docker container.

---

## âš¡ Architecture & Design

1. **Drogon Web Framework**:
   - Ranked #1 for C++ frameworks in the TechEmpower benchmarks.
   - Built on a non-blocking asynchronous event loop (IOCP on Windows, epoll on Linux, kqueue on macOS) via Trantor.
   - Zero-copy HTTP pipeline and multi-threaded event polling mapped to hardware concurrency.

2. **RapidJSON Serialization**:
   - SIMD-accelerated (SSE4.2/AVX) JSON parser and generator.
   - In-place parsing and streaming serialization directly into response buffers without intermediate string allocations.

3. **Asynchronous PostgreSQL Connection Pooling**:
   - Drogon native async DB client (`DbClient`) with connection pooling (20 pre-warmed connections by default).
   - Parameterized queries with prepared statements to prevent SQL injection and minimize query parse overhead.

4. **Containerized PostgreSQL Database**:
   - Alpine-based PostgreSQL 16 image tuned for low latency (`shared_buffers=256MB`, `work_mem=4MB`, `random_page_cost=1.1`).
   - Automated incremental schema migrations with transaction-wrapped up and down rollbacks via `scripts/migrate.ps1` / `scripts/migrate.sh`.

---

## ðŸ“ Project Structure

```text
â”œâ”€â”€ Makefile                       # Cross-platform shortcuts (make dev, make build, make test)
â”œâ”€â”€ CMakeLists.txt                 # Modern C++20 build definition & optimization flags
â”œâ”€â”€ CMakePresets.json              # Presets for Ninja / MSVC / vcpkg
â”œâ”€â”€ vcpkg.json                     # Dependency manifest (Drogon[postgres,orm], RapidJSON)
â”œâ”€â”€ config.json                    # Drogon server & connection pool runtime configuration
â”œâ”€â”€ docker-compose.yml             # Tuned PostgreSQL container definition
â”œâ”€â”€ migrations/                    # SQL schema & data migrations (up and down)
â”‚   â”œâ”€â”€ 0001_initial_schema.up.sql     # Base users table & indexes
â”‚   â”œâ”€â”€ 0001_initial_schema.down.sql   # Revert base table
â”‚   â”œâ”€â”€ 0002_seed_data.up.sql          # Initial test data
â”‚   â”œâ”€â”€ 0002_seed_data.down.sql        # Revert initial data
â”‚   â”œâ”€â”€ 0003_add_user_bio.up.sql       # Schema migration: add bio column
â”‚   â””â”€â”€ 0003_add_user_bio.down.sql     # Schema migration rollback
â”œâ”€â”€ include/api/                   # Public headers
â”‚   â”œâ”€â”€ common/                    # ApiResponse & RapidJsonHelper
â”‚   â”œâ”€â”€ controllers/               # HealthController & UserController
â”‚   â”œâ”€â”€ db/                        # DatabaseManager
â”‚   â”œâ”€â”€ models/                    # User model & DTOs
â”‚   â””â”€â”€ services/                  # UserService
â”œâ”€â”€ src/                           # Implementation source files
â”‚   â”œâ”€â”€ main.cpp                   # Application entrypoint
â”‚   â”œâ”€â”€ db/DatabaseManager.cpp
â”‚   â”œâ”€â”€ controllers/
â”‚   â””â”€â”€ services/
â”œâ”€â”€ scripts/                       # Automation scripts (start-db, migrate, test-api, build)
â””â”€â”€ tests/                         # Unit tests (RapidJSON, User serialization, validation)
```