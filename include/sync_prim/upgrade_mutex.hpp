#pragma once

#include <atomic>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <stdexcept>

namespace sync_prim
{

  // Forward declarations
  class upgrade_mutex;

  template <typename Mutex>
  class unique_lock;

  template <typename Mutex>
  class shared_lock;

  template <typename Mutex>
  class upgrade_lock;

  template <typename Mutex>
  class scoped_upgrade;

  /**
   * @class upgrade_mutex
   * @brief A synchronization primitive that allows multiple readers, a single
   * upgrader, and a single writer, with atomic transitions between lock states.
   *
   * This mutex manages three levels of access:
   * 1. Shared (Read): Allows multiple concurrent threads.
   * 2. Upgradeable (Privileged Read): A single thread can hold this lock, signaling
   * a potential intent to write. It can coexist with shared locks.
   * 3. Exclusive (Write): A single thread holds this lock, blocking all others.
   *
   * The state is managed by a single atomic integer, allowing for efficient
   * lock acquisition and release under low contention.
   */
  class upgrade_mutex
  {
  public:
    upgrade_mutex() : state_(0) {}

    upgrade_mutex(const upgrade_mutex &) = delete;
    upgrade_mutex &operator=(const upgrade_mutex &) = delete;

    // Exclusive locking
    void lock();
    void unlock();

    // Shared locking
    void lock_shared();
    void unlock_shared();

    // Upgradeable locking
    void lock_upgrade();
    void unlock_upgrade();

  private:
    friend class unique_lock<upgrade_mutex>;
    friend class shared_lock<upgrade_mutex>;
    friend class upgrade_lock<upgrade_mutex>;
    friend class scoped_upgrade<upgrade_mutex>;

    // --- Internal transition functions for lock guards ---
    void upgrade_to_unique();
    void unique_to_upgrade();
    void unique_to_shared();
    void scoped_upgrade_entry();
    void scoped_upgrade_exit();

    // --- State constants ---
    // The state is represented by a 32-bit atomic integer.
    // Bit 31: Exclusive write lock held
    // Bit 30: Upgradeable lock held
    // Bit 29: An upgrade to exclusive is pending (to starve new readers)
    // Bits 0-28: Count of shared readers
    static constexpr uint32_t WRITE_LOCKED_FLAG = 1u << 31;
    static constexpr uint32_t UPGRADE_LOCKED_FLAG = 1u << 30;
    static constexpr uint32_t UPGRADE_PENDING_FLAG = 1u << 29;
    static constexpr uint32_t READER_COUNT_MASK = ~(WRITE_LOCKED_FLAG | UPGRADE_LOCKED_FLAG | UPGRADE_PENDING_FLAG);
    static constexpr uint32_t ONE_READER = 1u;

    // --- Synchronization Primitives ---
    std::atomic<uint32_t> state_;
    std::mutex internal_mutex_;
    std::condition_variable gate1_; // For shared/upgrade waiters
    std::condition_variable gate2_; // For exclusive/upgrade-to-exclusive waiters
  };

  // --- Lock Guard Implementations ---

  /**
   * @brief A generic RAII lock guard base providing common functionality.
   */
  template <typename Mutex>
  class lock_guard_base
  {
  public:
    bool owns_lock() const noexcept { return p_mutex_ != nullptr; }
    explicit operator bool() const noexcept { return owns_lock(); }

    void release() noexcept { p_mutex_ = nullptr; }

    // Public accessor for the underlying mutex pointer.
    Mutex *mutex() const noexcept { return p_mutex_; }

  protected:
    lock_guard_base() noexcept : p_mutex_(nullptr) {}
    explicit lock_guard_base(Mutex &m) noexcept : p_mutex_(&m) {}
    lock_guard_base(lock_guard_base &&other) noexcept : p_mutex_(other.p_mutex_)
    {
      other.p_mutex_ = nullptr;
    }
    lock_guard_base &operator=(lock_guard_base &&other) noexcept
    {
      if (this != &other)
      {
        p_mutex_ = other.p_mutex_;
        other.p_mutex_ = nullptr;
      }
      return *this;
    }

    lock_guard_base(const lock_guard_base &) = delete;
    lock_guard_base &operator=(const lock_guard_base &) = delete;

    Mutex *p_mutex_;
  };

  /**
   * @brief An RAII wrapper for an exclusive lock on an upgrade_mutex.
   */
  template <>
  class unique_lock<upgrade_mutex> : public lock_guard_base<upgrade_mutex>
  {
  public:
    unique_lock() noexcept = default;
    explicit unique_lock(upgrade_mutex &m) : lock_guard_base(m) { p_mutex_->lock(); }
    ~unique_lock()
    {
      if (p_mutex_)
        p_mutex_->unlock();
    }

    unique_lock(unique_lock &&other) noexcept = default;
    unique_lock &operator=(unique_lock &&other) noexcept = default;

    // Atomic transition from an upgrade_lock
    explicit unique_lock(upgrade_lock<upgrade_mutex> &&other);
  };

  /**
   * @brief An RAII wrapper for a shared lock on an upgrade_mutex.
   */
  template <>
  class shared_lock<upgrade_mutex> : public lock_guard_base<upgrade_mutex>
  {
  public:
    shared_lock() noexcept = default;
    explicit shared_lock(upgrade_mutex &m) : lock_guard_base(m) { p_mutex_->lock_shared(); }
    ~shared_lock()
    {
      if (p_mutex_)
        p_mutex_->unlock_shared();
    }

    shared_lock(shared_lock &&other) noexcept = default;
    shared_lock &operator=(shared_lock &&other) noexcept = default;

    // Atomic transition from a unique_lock
    explicit shared_lock(unique_lock<upgrade_mutex> &&other);
  };

  /**
   * @brief An RAII wrapper for an upgradeable lock on an upgrade_mutex.
   */
  template <>
  class upgrade_lock<upgrade_mutex> : public lock_guard_base<upgrade_mutex>
  {
  public:
    upgrade_lock() noexcept = default;
    explicit upgrade_lock(upgrade_mutex &m) : lock_guard_base(m) { p_mutex_->lock_upgrade(); }
    ~upgrade_lock()
    {
      if (p_mutex_)
        p_mutex_->unlock_upgrade();
    }

    upgrade_lock(upgrade_lock &&other) noexcept = default;
    upgrade_lock &operator=(upgrade_lock &&other) noexcept = default;

    // Atomic transition from a unique_lock
    explicit upgrade_lock(unique_lock<upgrade_mutex> &&other);
  };

  /**
   * @brief A scoped RAII wrapper to temporarily upgrade an upgrade_lock to unique
   * and automatically downgrade upon destruction.
   */
  template <>
  class scoped_upgrade<upgrade_mutex>
  {
  public:
    explicit scoped_upgrade(upgrade_lock<upgrade_mutex> &lock) : p_mutex_(lock.mutex())
    {
      if (p_mutex_)
      {
        p_mutex_->scoped_upgrade_entry();
      }
    }

    ~scoped_upgrade()
    {
      if (p_mutex_)
      {
        p_mutex_->scoped_upgrade_exit();
      }
    }

    scoped_upgrade(const scoped_upgrade &) = delete;
    scoped_upgrade &operator=(const scoped_upgrade &) = delete;

  private:
    upgrade_mutex *p_mutex_;
  };

  // --- Atomic Transition Constructor Implementations ---

  inline unique_lock<upgrade_mutex>::unique_lock(upgrade_lock<upgrade_mutex> &&other)
      : lock_guard_base(std::move(other))
  {
    if (p_mutex_)
    {
      p_mutex_->upgrade_to_unique();
    }
  }

  inline upgrade_lock<upgrade_mutex>::upgrade_lock(unique_lock<upgrade_mutex> &&other)
      : lock_guard_base(std::move(other))
  {
    if (p_mutex_)
    {
      p_mutex_->unique_to_upgrade();
    }
  }

  inline shared_lock<upgrade_mutex>::shared_lock(unique_lock<upgrade_mutex> &&other)
      : lock_guard_base(std::move(other))
  {
    if (p_mutex_)
    {
      p_mutex_->unique_to_shared();
    }
  }

  // --- upgrade_mutex Method Implementations ---

  void upgrade_mutex::lock()
  {
    std::unique_lock<std::mutex> internal_lock(internal_mutex_);
    gate2_.wait(internal_lock, [this]
                {
        // Wait until no other locks are held
        uint32_t current_state = state_.load(std::memory_order_relaxed);
        if (current_state == 0) {
            // Attempt to acquire the lock
            return state_.compare_exchange_strong(current_state, WRITE_LOCKED_FLAG, std::memory_order_acquire, std::memory_order_relaxed);
        }
        return false; });
  }

  void upgrade_mutex::unlock()
  {
    // Atomically clear the write flag.
    state_.fetch_sub(WRITE_LOCKED_FLAG, std::memory_order_release);

    // Wake up ONE waiting writer on gate2. This prevents a thundering herd of writers.
    gate2_.notify_one();

    // Wake up ALL waiting readers and a potential upgrader on gate1.
    gate1_.notify_all();
  }

  void upgrade_mutex::lock_shared()
  {
    std::unique_lock<std::mutex> internal_lock(internal_mutex_);
    gate1_.wait(internal_lock, [this]
                {
        uint32_t current_state = state_.load(std::memory_order_relaxed);
        // Can acquire a read lock if there's no write lock and no pending upgrade
        if ((current_state & WRITE_LOCKED_FLAG) == 0 && (current_state & UPGRADE_PENDING_FLAG) == 0) {
            // Attempt to increment reader count
            uint32_t new_state = current_state + ONE_READER;
            return state_.compare_exchange_strong(current_state, new_state, std::memory_order_acquire, std::memory_order_relaxed);
        }
        return false; });
  }

  void upgrade_mutex::unlock_shared()
  {
    uint32_t old_state = state_.fetch_sub(ONE_READER, std::memory_order_release);
    // If we were the last lock holder (last reader and no upgrader), wake up a writer.
    if ((old_state & READER_COUNT_MASK) == ONE_READER && (old_state & UPGRADE_LOCKED_FLAG) == 0)
    {
      gate2_.notify_one();
    }
  }

  void upgrade_mutex::lock_upgrade()
  {
    std::unique_lock<std::mutex> internal_lock(internal_mutex_);
    gate1_.wait(internal_lock, [this]
                {
        uint32_t current_state = state_.load(std::memory_order_relaxed);
        // Can acquire if no write lock and no other upgrade lock is held
        if ((current_state & WRITE_LOCKED_FLAG) == 0 && (current_state & UPGRADE_LOCKED_FLAG) == 0) {
            uint32_t new_state = current_state | UPGRADE_LOCKED_FLAG;
            return state_.compare_exchange_strong(current_state, new_state, std::memory_order_acquire, std::memory_order_relaxed);
        }
        return false; });
  }

  void upgrade_mutex::unlock_upgrade()
  {
    uint32_t old_state = state_.fetch_sub(UPGRADE_LOCKED_FLAG, std::memory_order_release);

    // If releasing this upgrade lock makes the mutex completely free (no readers were present),
    // then a waiting exclusive writer can proceed.
    if ((old_state & READER_COUNT_MASK) == 0)
    {
      gate2_.notify_one();
    }

    // Notify waiting threads on gate1. This allows a new upgrader or waiting readers
    // to contend for the lock now that the previous upgrade lock has been released.
    gate1_.notify_all();
  }

  // --- Internal Transition Method Implementations ---

  void upgrade_mutex::upgrade_to_unique()
  {
    std::unique_lock<std::mutex> internal_lock(internal_mutex_);
    // Signal that an upgrade is pending to block new readers
    state_.fetch_or(UPGRADE_PENDING_FLAG, std::memory_order_acquire);

    // Wait until all current readers are finished
    gate2_.wait(internal_lock, [this]
                { return (state_.load(std::memory_order_relaxed) & READER_COUNT_MASK) == 0; });

    // Atomically swap upgrade and pending flags for the write flag
    state_.store(WRITE_LOCKED_FLAG, std::memory_order_release);
  }

  void upgrade_mutex::unique_to_upgrade()
  {
    // Atomically swap write flag for upgrade flag
    state_.store(UPGRADE_LOCKED_FLAG, std::memory_order_release);
    // Wake up any waiting readers
    gate1_.notify_all();
  }

  void upgrade_mutex::unique_to_shared()
  {
    // Atomically swap write flag for a single reader
    state_.store(ONE_READER, std::memory_order_release);
    // Wake up any waiting readers
    gate1_.notify_all();
  }

  void upgrade_mutex::scoped_upgrade_entry()
  {
    // This is identical to a full upgrade
    upgrade_to_unique();
  }

  void upgrade_mutex::scoped_upgrade_exit()
  {
    // This is identical to a downgrade to upgradeable
    unique_to_upgrade();
  }

} // namespace sync_prim
