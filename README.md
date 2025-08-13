# Upgradeable Mutex Library

A modern, header-only C++17 library providing an **upgradeable reader-writer mutex** (`upgrade_mutex`) with atomic lock transitions and RAII lock guards. This library supplements the standard `<mutex>` and `<shared_mutex>` by enabling safe, efficient upgrades from shared to exclusive locks—solving the classic "read-to-write upgrade" problem without deadlock or starvation.

---

## Features

- **Upgradeable Mutex (`upgrade_mutex`)**: Three lock levels—shared (read), upgradeable (privileged read), and exclusive (write).
- **RAII Lock Guards**:
  - `shared_lock<upgrade_mutex>`
  - `upgrade_lock<upgrade_mutex>`
  - `unique_lock<upgrade_mutex>`
  - `scoped_upgrade<upgrade_mutex>` (for temporary upgrades)
- **Atomic Lock Transitions**: Move-construct lock guards to atomically upgrade/downgrade lock types.
- **Header-only**: Just include a single header—no linking required.
- **C++17 Standard Library Only**: No external dependencies.
- **Well-tested**: Includes unit tests and benchmarks.

---

## Getting Started

### 1. Clone and Build

```sh
git clone <repo-url> sync_prim
cd sync_prim
make build
```

### 2. Usage

Just include the header in your project:

```cpp
#include "sync_prim/upgrade_mutex.hpp"
```

See [src/bank_account_example.cpp](src/bank_account_example.cpp) for a real-world usage example.

---

## API Overview

### upgrade_mutex

A synchronization primitive supporting:

- **Multiple Readers**: Many threads can hold `shared_lock` concurrently.
- **Single Upgrader**: Only one thread can hold an `upgrade_lock` at a time (can coexist with readers).
- **Exclusive Writer**: Only one thread can hold a `unique_lock` at a time (excludes all others).
- **Upgrade Priority**: Prevents writer starvation by blocking new readers when an upgrade is pending.

### Lock Guards

- `sync_prim::shared_lock<upgrade_mutex>`: Shared (read) access.
- `sync_prim::upgrade_lock<upgrade_mutex>`: Upgradeable access.
- `sync_prim::unique_lock<upgrade_mutex>`: Exclusive (write) access.
- `sync_prim::scoped_upgrade<upgrade_mutex>`: Temporarily upgrade an `upgrade_lock` to exclusive within a scope.

#### Example: Upgradeable Lock Pattern

```cpp
sync_prim::upgrade_mutex mtx;

// Acquire upgradeable lock
sync_prim::upgrade_lock<sync_prim::upgrade_mutex> u_lock(mtx);

// ... read-only operations ...

// Upgrade to exclusive
sync_prim::unique_lock<sync_prim::upgrade_mutex> x_lock(std::move(u_lock));
// ... write operations ...
```

#### Example: Scoped Upgrade

```cpp
sync_prim::upgrade_lock<sync_prim::upgrade_mutex> u_lock(mtx);
{
    sync_prim::scoped_upgrade<sync_prim::upgrade_mutex> s_upgrade(u_lock);
    // ... exclusive access in this scope ...
}
// Automatically downgrades to upgrade_lock here
```

---

## Building & Testing

### Build

```sh
make build
```

### Run Tests

```sh
make run_tests
```

Or, from the build directory:

```sh
cd build
ctest --verbose
```

### Run Benchmarks

```sh
./build/run_benchmarks
```

---

## File Structure

- [`include/sync_prim/upgrade_mutex.hpp`](include/sync_prim/upgrade_mutex.hpp): Main header-only library.
- [`src/bank_account_example.cpp`](src/bank_account_example.cpp): Example usage.
- [`src/benchmark_upgrade_mutex.cpp`](src/benchmark_upgrade_mutex.cpp): Performance benchmarks.
- [`tests/test_upgrade_mutex.cpp`](tests/test_upgrade_mutex.cpp): Unit tests.
- [`DESIGN.md`](DESIGN.md): Internal design details.
- [`REQUIREMENTS.md`](REQUIREMENTS.md): Requirements specification.

---

## Installation

To install the header to a custom prefix:

```sh
make install INSTALL_PREFIX=~/.local
```

This will copy the header to `~/.local/include/sync_prim/`.

---

## License

MIT