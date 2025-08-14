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
#include <vector>
#include <thread>
#include <string>
#include <iomanip>
#include <random>

/**
 * @class BankAccount
 * @brief A thread-safe bank account protected by an upgrade_mutex.
 */
class BankAccount
{
public:
  BankAccount(std::string name, double initial_balance)
      : account_name_(std::move(name)), balance_(initial_balance)
  {
    std::cout << "Opened account '" << account_name_ << "' with balance: $"
              << std::fixed << std::setprecision(2) << balance_ << std::endl;
  }

  /**
   * @brief Reads the balance. A pure read operation.
   * Uses a shared_lock, allowing multiple threads to check balances concurrently.
   */
  double get_balance()
  {
    sync_prim::shared_lock<sync_prim::upgrade_mutex> lock(mtx_);
    return balance_;
  }

  /**
   * @brief Deposits money. A pure write operation.
   * Uses a unique_lock to ensure exclusive access.
   */
  void deposit(double amount)
  {
    sync_prim::unique_lock<sync_prim::upgrade_mutex> lock(mtx_);
    balance_ += amount;
    std::cout << "Deposited $" << amount << " into '" << account_name_
              << "'. New balance: $" << balance_ << std::endl;
  }

  /**
   * @brief Withdraws money. A pure write operation.
   * Uses a unique_lock to ensure exclusive access.
   */
  bool withdraw(double amount)
  {
    sync_prim::unique_lock<sync_prim::upgrade_mutex> lock(mtx_);
    if (balance_ >= amount)
    {
      balance_ -= amount;
      std::cout << "Withdrew $" << amount << " from '" << account_name_
                << "'. New balance: $" << balance_ << std::endl;
      return true;
    }
    std::cout << "Withdrawal of $" << amount << " from '" << account_name_
              << "' failed. Insufficient funds." << std::endl;
    return false;
  }

  /**
   * @brief Checks if a large purchase can be made and logs it.
   * This is a perfect use case for an upgradeable lock. It first reads the
   * balance, and only if the condition is met does it upgrade to a write lock.
   * This avoids taking a slow exclusive lock unnecessarily.
   */
  void log_large_purchase_if_possible(double amount)
  {
    sync_prim::upgrade_lock<sync_prim::upgrade_mutex> u_lock(mtx_);

    std::cout << "[AUDIT] Checking if '" << account_name_ << "' can afford $" << amount << std::endl;

    if (balance_ > amount)
    {
      // The purchase is possible. Upgrade to write a log entry (simulated by cout).
      // We use the safe and convenient scoped_upgrade wrapper.
      sync_prim::scoped_upgrade<sync_prim::upgrade_mutex> s_upgrade(u_lock);

      // Now we have an exclusive lock within this scope.
      std::cout << "[AUDIT] SUCCESS: '" << account_name_ << "' with balance $"
                << balance_ << " can afford purchase of $" << amount << ". Logging event." << std::endl;
    }
    else
    {
      std::cout << "[AUDIT] FAILED: '" << account_name_ << "' cannot afford $" << amount << std::endl;
    }
    // The scoped_upgrade automatically downgrades here, or if the `if` was skipped,
    // the upgrade_lock is simply released.
  }

private:
  std::string account_name_;
  double balance_;
  sync_prim::upgrade_mutex mtx_;
};

int main()
{
  BankAccount my_account("Robotics Vision Fund", 1000.00);

  std::vector<std::thread> threads;
  std::mt19937 rng(std::random_device{}());
  std::uniform_real_distribution<double> deposit_dist(10.0, 50.0);
  std::uniform_real_distribution<double> withdraw_dist(20.0, 70.0);

  // Spawn several threads to perform concurrent operations
  for (int i = 0; i < 3; ++i)
  {
    threads.emplace_back([&]()
                         {
            my_account.deposit(deposit_dist(rng));
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            my_account.withdraw(withdraw_dist(rng)); });
  }

  // Spawn audit threads that use the upgradeable lock
  for (int i = 0; i < 2; ++i)
  {
    threads.emplace_back([&]()
                         {
            my_account.log_large_purchase_if_possible(500.00);
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
            my_account.log_large_purchase_if_possible(1200.00); });
  }

  // Spawn a thread that just reads the balance
  threads.emplace_back([&]()
                       {
        for(int i=0; i<5; ++i) {
            std::cout << "Balance check thread sees: $" << my_account.get_balance() << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(200));
        } });

  for (auto &t : threads)
  {
    t.join();
  }

  std::cout << "\nAll transactions complete." << std::endl;
  std::cout << "Final balance of '"
            << "Robotics Vision Fund"
            << "': $"
            << my_account.get_balance() << std::endl;

  return 0;
}
