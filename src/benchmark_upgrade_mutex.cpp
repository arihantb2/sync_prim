#include "sync_prim/upgrade_mutex.hpp"
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <iomanip>
#include <functional>

// A simple data structure to be protected by the mutexes
struct ProtectedData
{
  long long counter = 0;
};

// --- Benchmark Runner ---
void run_benchmark(const std::string &name, std::function<void()> benchmark_func)
{
  std::cout << "Running benchmark: " << std::left << std::setw(45) << name << "... " << std::flush;
  auto start = std::chrono::high_resolution_clock::now();
  benchmark_func();
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<double> duration = end - start;
  std::cout << "Finished in " << std::fixed << std::setprecision(4) << duration.count() << "s" << std::endl;
}

// ===================================================================
//                        BENCHMARK SCENARIOS
// ===================================================================

// --- Scenario 1: Read-Heavy Workload (95% reads, 5% writes) ---
template <typename Mutex>
void read_heavy_benchmark(Mutex &mtx, ProtectedData &data)
{
  const int num_threads = 16;
  const int ops_per_thread = 10000;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([&, i]()
                         {
            for (int op = 0; op < ops_per_thread; ++op) {
                if (i == 0 && op % 20 == 0) { // 1 writer thread, 5% of its ops are writes
                    if constexpr (std::is_same_v<Mutex, sync_prim::upgrade_mutex> || std::is_same_v<Mutex, std::shared_mutex>) {
                        std::unique_lock lock(mtx);
                        data.counter++;
                    } else { // std::mutex
                        std::lock_guard lock(mtx);
                        data.counter++;
                    }
                } else { // All other ops are reads
                    if constexpr (std::is_same_v<Mutex, sync_prim::upgrade_mutex>) {
                        sync_prim::shared_lock<sync_prim::upgrade_mutex> lock(mtx);
                        volatile long long val = data.counter; (void)val;
                    } else if constexpr (std::is_same_v<Mutex, std::shared_mutex>) {
                        std::shared_lock lock(mtx);
                        volatile long long val = data.counter; (void)val;
                    } else { // std::mutex
                        std::lock_guard lock(mtx);
                        volatile long long val = data.counter; (void)val;
                    }
                }
            } });
  }
  for (auto &t : threads)
    t.join();
}

// --- Scenario 2: Write-Heavy Workload (50% reads, 50% writes) ---
template <typename Mutex>
void write_heavy_benchmark(Mutex &mtx, ProtectedData &data)
{
  const int num_threads = 8;
  const int ops_per_thread = 10000;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([&, i]()
                         {
            for (int op = 0; op < ops_per_thread; ++op) {
                if (op % 2 == 0) { // 50% writes
                    if constexpr (std::is_same_v<Mutex, sync_prim::upgrade_mutex> || std::is_same_v<Mutex, std::shared_mutex>) {
                        std::unique_lock lock(mtx);
                        data.counter++;
                    } else {
                        std::lock_guard lock(mtx);
                        data.counter++;
                    }
                } else { // 50% reads
                     if constexpr (std::is_same_v<Mutex, sync_prim::upgrade_mutex>) {
                        sync_prim::shared_lock<sync_prim::upgrade_mutex> lock(mtx);
                        volatile long long val = data.counter; (void)val;
                    } else if constexpr (std::is_same_v<Mutex, std::shared_mutex>) {
                        std::shared_lock lock(mtx);
                        volatile long long val = data.counter; (void)val;
                    } else {
                        std::lock_guard lock(mtx);
                        volatile long long val = data.counter; (void)val;
                    }
                }
            } });
  }
  for (auto &t : threads)
    t.join();
}

// --- Scenario 3: Upgrade-Heavy Workload (Read, then maybe write) ---
void upgrade_heavy_benchmark(sync_prim::upgrade_mutex &mtx, ProtectedData &data)
{
  const int num_threads = 8;
  const int ops_per_thread = 10000;
  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i)
  {
    threads.emplace_back([&]()
                         {
            for (int op = 0; op < ops_per_thread; ++op) {
                sync_prim::upgrade_lock<sync_prim::upgrade_mutex> u_lock(mtx);
                if (data.counter % 10 == 0) {
                    sync_prim::scoped_upgrade<sync_prim::upgrade_mutex> s_lock(u_lock);
                    data.counter++;
                }
            } });
  }
  for (auto &t : threads)
    t.join();
}

int main()
{
  std::cout << "--- Starting Mutex Performance Benchmarks ---" << std::endl;

  // --- Read-Heavy ---
  std::cout << "\n--- SCENARIO: READ-HEAVY (95% Reads) ---" << std::endl;
  {
    sync_prim::upgrade_mutex mtx;
    ProtectedData data;
    run_benchmark("upgrade_mutex (read-heavy)", [&]()
                  { read_heavy_benchmark(mtx, data); });
  }
  {
    std::shared_mutex mtx;
    ProtectedData data;
    run_benchmark("std::shared_mutex (read-heavy)", [&]()
                  { read_heavy_benchmark(mtx, data); });
  }
  {
    std::mutex mtx;
    ProtectedData data;
    run_benchmark("std::mutex (read-heavy)", [&]()
                  { read_heavy_benchmark(mtx, data); });
  }

  // --- Write-Heavy ---
  std::cout << "\n--- SCENARIO: WRITE-HEAVY (50% Writes) ---" << std::endl;
  {
    sync_prim::upgrade_mutex mtx;
    ProtectedData data;
    run_benchmark("upgrade_mutex (write-heavy)", [&]()
                  { write_heavy_benchmark(mtx, data); });
  }
  {
    std::shared_mutex mtx;
    ProtectedData data;
    run_benchmark("std::shared_mutex (write-heavy)", [&]()
                  { write_heavy_benchmark(mtx, data); });
  }
  {
    std::mutex mtx;
    ProtectedData data;
    run_benchmark("std::mutex (write-heavy)", [&]()
                  { write_heavy_benchmark(mtx, data); });
  }

  // --- Upgrade-Heavy ---
  std::cout << "\n--- SCENARIO: UPGRADE-HEAVY (Read, Conditionally Write) ---" << std::endl;
  {
    sync_prim::upgrade_mutex mtx;
    ProtectedData data;
    run_benchmark("upgrade_mutex (upgrade-heavy)", [&]()
                  { upgrade_heavy_benchmark(mtx, data); });
  }

  std::cout << "\n--- Benchmarks Complete ---" << std::endl;

  return 0;
}
