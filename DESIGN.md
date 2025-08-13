# Design Document: Upgradeable Mutex Library

## 1. Internal State Representation

The state of the `upgrade_mutex` will be managed by a single `std::atomic<uint32_t>` member named `state_`. This allows for lock-free reads of the state in some cases and reduces the scope of critical sections protected by a heavier `std::mutex`.

The 32 bits of `state_` will be used as a bitfield to track four distinct properties:

- **WRITE_LOCKED_FLAG** (Bit 31): Set if a thread holds an exclusive (write) lock.
- **UPGRADE_LOCKED_FLAG** (Bit 30): Set if a thread holds an upgradeable lock.
- **UPGRADE_PENDING_FLAG** (Bit 29): Set when an upgradeable lock holder is waiting to upgrade. This flag blocks new readers from acquiring a lock, preventing writer starvation.
- **READER_COUNT** (Bits 0-28): A counter for the number of threads holding a shared (read) lock.

Conceptual representation:

```
| 1b Write | 1b Upgrade | 1b Pending | 29 bits (Readers) |
+----------+------------+------------+-------------------+
```

## 2. Synchronization Primitives

- `state_`: `std::atomic<uint32_t>` — The core state variable.
- `internal_mutex_`: `std::mutex` — Protects access to the condition variables and orchestrates complex state changes that cannot be handled by a single atomic operation.
- `gate1_`: `std::condition_variable` — Used to signal threads waiting for a shared or upgradeable lock.
- `gate2_`: `std::condition_variable` — Used to signal threads waiting for an exclusive lock or waiting to complete an upgrade.

## 3. Locking Logic

### 3.1. `lock_shared()`

- **Condition:** `WRITE_LOCKED_FLAG` and `UPGRADE_PENDING_FLAG` are not set.
- **Action:** Atomically increment `READER_COUNT`.
- **Wait on:** `gate1_` if the condition is not met.

### 3.2. `lock_upgrade()`

- **Condition:** `WRITE_LOCKED_FLAG` and `UPGRADE_LOCKED_FLAG` are not set.
- **Action:** Atomically set `UPGRADE_LOCKED_FLAG`.
- **Wait on:** `gate1_` if the condition is not met.

### 3.3. `lock()` (Exclusive)

- **Condition:** `state_` is 0 (no readers, no upgrader, no writer).
- **Action:** Atomically set `WRITE_LOCKED_FLAG`.
- **Wait on:** `gate2_` if the condition is not met.

## 4. Unlocking Logic

### 4.1. `unlock_shared()`

- Atomically decrement `READER_COUNT`.
- If this was the last reader and no upgrader is present, notify `gate2_` to wake a potential writer.

### 4.2. `unlock_upgrade()`

- Atomically clear `UPGRADE_LOCKED_FLAG`.
- Notify all waiters on `gate1_` to wake potential readers and upgrade lock requesters.
- If no readers are present, notify `gate2_` to wake a potential writer.

### 4.3. `unlock()` (Exclusive)

- Atomically clear `WRITE_LOCKED_FLAG`.
- Notify all waiting threads on `gate1_` to allow waiting readers and a potential upgrader to contend for the lock.

## 5. Atomic Transition Logic

### 5.1. `upgrade_to_unique()`

- Lock `internal_mutex_`.
- Atomically set `UPGRADE_PENDING_FLAG` on `state_`. This immediately blocks any new readers.
- Wait on `gate2_` until `READER_COUNT` becomes 0.
- Atomically overwrite `state_` with just the `WRITE_LOCKED_FLAG`.
- Release `internal_mutex_`.

### 5.2. `unique_to_upgrade()`

- Atomically overwrite `state_` with just the `UPGRADE_LOCKED_FLAG`. This operation is non-blocking.
- Notify all waiters on `gate1_` since readers can now proceed.

### 5.3. `unique_to_shared()`

- Atomically overwrite `state_` with a reader count of 1. This is non-blocking.
- Notify all waiters on `gate1_`.

## 6. Scoped Upgrade Logic

The `scoped_upgrade` class provides a higher-level RAII wrapper.

- **Constructor (`scoped_upgrade_entry()`):**
  - Calls `upgrade_to_unique()` to acquire the exclusive lock.
- **Destructor (`scoped_upgrade_exit()`):**
  - Calls `unique_to_upgrade()` to atomically downgrade the lock back to upgradeable.