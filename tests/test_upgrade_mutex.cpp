/*
 * MIT License
 * Copyright (c) 2024 Arihant Lunawat <arihantb2@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "sync_prim/upgrade_mutex.hpp"
#include <iostream>
#include <cassert>
#include <thread>

// --- Test Runner Helper ---
void run_test(void (*test_func)(), const std::string &test_name)
{
  try
  {
    test_func();
    std::cout << "[PASS] " << test_name << std::endl;
  }
  catch (const std::exception &e)
  {
    std::cerr << "[FAIL] " << test_name << " - Exception: " << e.what() << std::endl;
  }
  catch (...)
  {
    std::cerr << "[FAIL] " << test_name << " - Unknown exception" << std::endl;
  }
}

// ===================================================================
//                        CORE LOGIC TESTS
// ===================================================================

void test_exclusive_lock()
{
  sync_prim::upgrade_mutex mtx;
  sync_prim::unique_lock<sync_prim::upgrade_mutex> lock(mtx);
  assert(lock.owns_lock());
}

void test_shared_lock()
{
  sync_prim::upgrade_mutex mtx;
  sync_prim::shared_lock<sync_prim::upgrade_mutex> lock1(mtx);
  sync_prim::shared_lock<sync_prim::upgrade_mutex> lock2(mtx);
  assert(lock1.owns_lock());
  assert(lock2.owns_lock());
}

void test_upgrade_lock()
{
  sync_prim::upgrade_mutex mtx;
  sync_prim::upgrade_lock<sync_prim::upgrade_mutex> lock(mtx);
  assert(lock.owns_lock());
}

void test_exclusive_blocks_others()
{
  sync_prim::upgrade_mutex mtx;
  sync_prim::unique_lock<sync_prim::upgrade_mutex> x_lock(mtx);

  std::atomic<bool> thread_finished = false;
  std::thread t([&]()
                {
        // This thread should block
        sync_prim::shared_lock<sync_prim::upgrade_mutex> s_lock(mtx);
        thread_finished = true; });

  std::this_thread::sleep_for(std::chrono::milliseconds(50));
  assert(!thread_finished); // The other thread should be blocked

  x_lock.release(); // Manually release the lock
  mtx.unlock();

  t.join();
  assert(thread_finished); // Now it should have finished
}

void test_upgrade_allows_readers()
{
  sync_prim::upgrade_mutex mtx;
  sync_prim::upgrade_lock<sync_prim::upgrade_mutex> u_lock(mtx);
  assert(u_lock.owns_lock());

  sync_prim::shared_lock<sync_prim::upgrade_mutex> s_lock(mtx);
  assert(s_lock.owns_lock());
}

// ===================================================================
//                        TRANSITION TESTS
// ===================================================================

void test_upgrade_downgrade_cycle()
{
  sync_prim::upgrade_mutex mtx;
  int data = 0;

  // 1. Acquire upgrade lock
  sync_prim::upgrade_lock<sync_prim::upgrade_mutex> u_lock(mtx);
  assert(u_lock.owns_lock());
  data = 1;

  // 2. Upgrade to unique lock
  sync_prim::unique_lock<sync_prim::upgrade_mutex> x_lock(std::move(u_lock));
  assert(x_lock.owns_lock());
  assert(!u_lock.owns_lock());
  data = 2;

  // 3. Downgrade back to upgrade lock
  u_lock = sync_prim::upgrade_lock<sync_prim::upgrade_mutex>(std::move(x_lock));
  assert(u_lock.owns_lock());
  assert(!x_lock.owns_lock());
  assert(data == 2);
}

void test_downgrade_to_shared()
{
  sync_prim::upgrade_mutex mtx;
  int data = 0;

  // 1. Acquire unique lock
  sync_prim::unique_lock<sync_prim::upgrade_mutex> x_lock(mtx);
  data = 1;

  // 2. Downgrade to shared lock
  sync_prim::shared_lock<sync_prim::upgrade_mutex> s_lock(std::move(x_lock));
  assert(s_lock.owns_lock());
  assert(!x_lock.owns_lock());
  assert(data == 1);

  // 3. Prove another shared lock can be acquired
  sync_prim::shared_lock<sync_prim::upgrade_mutex> s_lock2(mtx);
  assert(s_lock2.owns_lock());
}

void test_scoped_upgrade()
{
  sync_prim::upgrade_mutex mtx;
  int data = 0;

  sync_prim::upgrade_lock<sync_prim::upgrade_mutex> u_lock(mtx);
  assert(u_lock.owns_lock());
  data = 1;

  {
    // Temporarily upgrade
    sync_prim::scoped_upgrade<sync_prim::upgrade_mutex> s_upgrade(u_lock);
    data = 2;
  } // Downgrade happens automatically here

  // u_lock should still be valid
  assert(u_lock.owns_lock());
  assert(data == 2);
}

int main()
{
  std::cout << "--- Running Core Logic Tests ---" << std::endl;
  run_test(test_exclusive_lock, "Exclusive lock acquisition");
  run_test(test_shared_lock, "Shared lock acquisition");
  run_test(test_upgrade_lock, "Upgrade lock acquisition");
  run_test(test_exclusive_blocks_others, "Exclusive lock blocks others");
  run_test(test_upgrade_allows_readers, "Upgrade lock allows readers");

  std::cout << "\n--- Running Transition Tests ---" << std::endl;
  run_test(test_upgrade_downgrade_cycle, "Upgrade -> Unique -> Upgrade cycle");
  run_test(test_downgrade_to_shared, "Unique -> Shared downgrade");
  run_test(test_scoped_upgrade, "Scoped upgrade and automatic downgrade");

  return 0;
}
