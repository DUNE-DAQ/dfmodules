/**
 * @file WriteRetry_test.cxx Unit tests of write_with_retries()
 *
 * This is part of the DUNE DAQ Application Framework, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#include "dfmodules/WriteRetry.hpp"

#define BOOST_TEST_MODULE WriteRetry_test // NOLINT

#include "boost/test/unit_test.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>

using namespace dunedaq::dfmodules;

namespace {
const WriteRetryPolicy s_fast_policy{ std::chrono::microseconds(1), std::chrono::microseconds(10), 2.0 };
}

BOOST_AUTO_TEST_SUITE(WriteRetry_test)

BOOST_AUTO_TEST_CASE(SucceedsFirstTime)
{
  std::atomic<bool> keep_trying{ true };
  int calls = 0;
  int problems = 0;
  BOOST_REQUIRE(write_with_retries(
    [&]() { ++calls; }, [&](const RetryableDataStoreProblem&) { ++problems; }, s_fast_policy, keep_trying));
  BOOST_REQUIRE_EQUAL(calls, 1);
  BOOST_REQUIRE_EQUAL(problems, 0);
}

BOOST_AUTO_TEST_CASE(RetriesRetryableProblems)
{
  std::atomic<bool> keep_trying{ true };
  int calls = 0;
  int problems = 0;
  auto write = [&]() {
    if (++calls < 4) {
      throw RetryableDataStoreProblem(ERS_HERE, "test", "a test write");
    }
  };
  BOOST_REQUIRE(
    write_with_retries(write, [&](const RetryableDataStoreProblem&) { ++problems; }, s_fast_policy, keep_trying));
  BOOST_REQUIRE_EQUAL(calls, 4);
  BOOST_REQUIRE_EQUAL(problems, 3);
}

BOOST_AUTO_TEST_CASE(StopsWhenAskedTo)
{
  std::atomic<bool> keep_trying{ true };
  int calls = 0;
  auto write = [&]() {
    ++calls;
    keep_trying = false; // e.g. the run is stopped while retrying
    throw RetryableDataStoreProblem(ERS_HERE, "test", "a test write");
  };
  BOOST_REQUIRE(!write_with_retries(write, [](const RetryableDataStoreProblem&) {}, s_fast_policy, keep_trying));
  BOOST_REQUIRE_EQUAL(calls, 1);
}

// A stop requested while waiting before a retry prevents the retry: write() is not called again
BOOST_AUTO_TEST_CASE(StopDuringWaitPreventsAnotherAttempt)
{
  const WriteRetryPolicy slow_policy{ std::chrono::milliseconds(500), std::chrono::milliseconds(500), 1.0 };
  std::atomic<bool> keep_trying{ true };
  std::atomic<int> calls{ 0 };
  auto write = [&]() {
    ++calls;
    throw RetryableDataStoreProblem(ERS_HERE, "test", "a test write");
  };

  // stops the retries while write_with_retries is in its 500 ms wait after the first attempt
  std::thread stopper([&]() {
    while (calls.load() == 0) {
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    keep_trying = false;
  });
  bool written = write_with_retries(write, [](const RetryableDataStoreProblem&) {}, slow_policy, keep_trying);
  stopper.join();

  BOOST_REQUIRE(!written);
  BOOST_REQUIRE_EQUAL(calls.load(), 1);
}

BOOST_AUTO_TEST_CASE(OtherExceptionsArePassedOn)
{
  std::atomic<bool> keep_trying{ true };
  int calls = 0;
  auto write = [&]() {
    ++calls;
    throw std::runtime_error("not retryable");
  };
  BOOST_REQUIRE_THROW(
    write_with_retries(write, [](const RetryableDataStoreProblem&) {}, s_fast_policy, keep_trying), std::runtime_error);
  BOOST_REQUIRE_EQUAL(calls, 1);
}

BOOST_AUTO_TEST_CASE(ZeroIncreaseFactorStillWaits)
{
  // a factor of 0 (the schema default) used to make every wait after the first 0 microseconds
  const WriteRetryPolicy policy{ std::chrono::microseconds(50), std::chrono::microseconds(1000), 0.0 };
  std::atomic<bool> keep_trying{ true };
  int calls = 0;
  auto write = [&]() {
    if (++calls < 5) {
      throw RetryableDataStoreProblem(ERS_HERE, "test", "a test write");
    }
  };
  auto start = std::chrono::steady_clock::now();
  BOOST_REQUIRE(write_with_retries(write, [](const RetryableDataStoreProblem&) {}, policy, keep_trying));
  BOOST_REQUIRE(std::chrono::steady_clock::now() - start >= std::chrono::microseconds(4 * 50));
}

BOOST_AUTO_TEST_SUITE_END()
