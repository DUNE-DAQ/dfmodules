/**
 * @file WriteRetry.hpp Retrying DataStore writes that fail with RetryableDataStoreProblem
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_INCLUDE_DFMODULES_WRITERETRY_HPP_
#define DFMODULES_INCLUDE_DFMODULES_WRITERETRY_HPP_

#include "dfmodules/DataStore.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <thread>
#include <utility>

namespace dunedaq::dfmodules {

/**
 * @brief How long to wait between attempts: first min_wait, then growing by increase_factor
 * each time, up to max_wait. Factors below 1 are treated as 1 (a constant wait).
 */
struct WriteRetryPolicy
{
  std::chrono::microseconds min_wait{ 1 };
  std::chrono::microseconds max_wait{ 1'000'000 };
  double increase_factor{ 2.0 };
};

/**
 * @brief Calls write() until it succeeds or keep_trying becomes false. Each time write() throws
 * RetryableDataStoreProblem, on_retryable_problem(issue) is called (e.g. to report it), and the
 * next attempt waits as the policy says. keep_trying is checked before and after each wait, so a
 * stop requested during a wait prevents another attempt. Any other exception is passed on to the caller.
 * @return true if write() succeeded, false if keep_trying became false first
 */
template<typename WriteFunction, typename ProblemHandler>
bool
write_with_retries(WriteFunction&& write,
                   ProblemHandler&& on_retryable_problem,
                   const WriteRetryPolicy& policy,
                   const std::atomic<bool>& keep_trying)
{
  const double factor = std::max(policy.increase_factor, 1.0);
  auto wait = std::max(policy.min_wait, std::chrono::microseconds{ 1 });
  while (true) {
    try {
      write();
      return true;
    } catch (const RetryableDataStoreProblem& issue) {
      on_retryable_problem(issue);
    }
    if (!keep_trying.load()) {
      return false;
    }
    wait = std::min(wait, policy.max_wait);
    std::this_thread::sleep_for(wait);
    if (!keep_trying.load()) {
      return false;
    }
    wait = std::chrono::microseconds{ static_cast<std::chrono::microseconds::rep>(static_cast<double>(wait.count()) *
                                                                                  factor) };
  }
}

} // namespace dunedaq::dfmodules

#endif // DFMODULES_INCLUDE_DFMODULES_WRITERETRY_HPP_
