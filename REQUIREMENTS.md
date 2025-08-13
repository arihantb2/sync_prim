# Requirements: Upgradeable Mutex Library

## 1. Overview

This document specifies the requirements for a header-only C++ library that provides an **upgradeable reader-writer mutex**, `upgrade_mutex`. This library will supplement the standard `<mutex>` and `<shared_mutex>` headers by offering a solution to the common problem of safely and efficiently upgrading a read lock to a write lock without risking deadlock or starvation.

The library will be self-contained in a single header file for ease of integration and will depend only on the C++17 standard library.

## 2. Core Components

### 2.1. `upgrade_mutex`

A synchronization primitive that manages three levels of access:

- **Shared (Read) Access:** Allows multiple concurrent threads to hold a lock concurrently.
- **Upgradeable (Privileged Read) Access:** Allows only one thread at a time to hold this type of lock. This lock can coexist with multiple shared locks. A thread holding an upgradeable lock signals an intent to potentially write in the future.
- **Exclusive (Write) Access:** Allows only one thread at a time to hold the lock, excluding all other threads (shared or upgradeable).

### 2.2. Lock Guards

Standard RAII-style lock guards will be provided to manage the lifetime of the locks:

- `shared_lock<upgrade_mutex>`: Acquires shared ownership.
- `upgrade_lock<upgrade_mutex>`: Acquires upgradeable ownership.
- `unique_lock<upgrade_mutex>`: Acquires exclusive ownership.
- `scoped_upgrade<upgrade_mutex>`: A convenience wrapper to manage a temporary, scoped upgrade from an `upgrade_lock` to a `unique_lock` and back.

## 3. Functional Requirements

### 3.1. `upgrade_mutex` Behavior

- **Multiple Readers:** The mutex must allow multiple threads to acquire a `shared_lock` simultaneously, provided no thread holds an `exclusive_lock`.
- **Single Upgrader:** The mutex must allow only one thread to acquire an `upgrade_lock` at any given time.
- **Coexistence of Upgrader and Readers:** A thread holding an `upgrade_lock` must not block other threads from acquiring a `shared_lock`.
- **Exclusive Writer:** A thread attempting to acquire a `unique_lock` must wait until no other threads hold any type of lock (shared or upgrade).
- **Upgrade Priority:** When a thread holding an `upgrade_lock` attempts to upgrade to a `unique_lock`, the mutex must block any new threads from acquiring `shared_locks` to prevent writer starvation. Existing shared lock holders are allowed to finish.

### 3.2. Atomic Lock Transitions

The library must provide mechanisms to atomically transition between lock types without releasing the mutex, which would create a race condition.

- **Ownership Transfer:** The primary lock guards (`unique_lock`, `upgrade_lock`, `shared_lock`) will support atomic transitions via move constructors, allowing for permanent ownership transfer between lock types (e.g., `unique_lock(upgrade_lock&&)`).
- **Scoped Upgrade:** The `scoped_upgrade` wrapper will provide a higher-level, safer mechanism for temporary upgrades. It will take a reference to an `upgrade_lock`, perform the upgrade on construction, and automatically perform the downgrade to an `upgrade_lock` on destruction.

### 3.3. API and Interface

- **Header-Only:** The entire library must be contained within a single `.hpp` header file.
- **Namespace:** All components of the library will be placed within a distinct namespace (`sync_prim`).
- **Standard Compliance:** The implementation must use only C++17 features and standard library components.

## 4. Non-Functional Requirements

- **Performance:** The implementation should be efficient, minimizing contention and avoiding unnecessary spinning.
- **Fairness:** The design must prevent starvation of writers waiting for an upgrade. The upgrade priority mechanism (3.1.5) is key to this.
- **Clarity and Maintainability:** The code must be well-documented.