/**
 * @file ReceivedDataflowStatus.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_SRC_DFMODULES_RECEIVEDDATAFLOWSTATUS_HPP_
#define DFMODULES_SRC_DFMODULES_RECEIVEDDATAFLOWSTATUS_HPP_

#include "dfmessages/DataflowStatus.hpp"

#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

namespace dunedaq::dfmodules {

struct ReceivedDataflowStatus
{
  dfmessages::DataflowStatus status;
  std::chrono::steady_clock::time_point received_time;
  std::mutex status_update_mutex;
  std::condition_variable status_update_cv;
  std::shared_ptr<std::jthread> timeout_thread;
  std::atomic<bool> status_updated{ true };
  std::chrono::milliseconds status_timeout_ms{ 5000 };

  ReceivedDataflowStatus(dfmessages::DataflowStatus s, std::chrono::milliseconds status_timeout)
    : status(s)
    , received_time(std::chrono::steady_clock::now())
    , timeout_thread(
        std::make_shared<std::jthread>(std::bind_front(&ReceivedDataflowStatus::dataflow_status_timeout_proc, this)))
    , status_timeout_ms(status_timeout)
  {
  }

  ~ReceivedDataflowStatus()
  {
    if (timeout_thread && timeout_thread->joinable()) {
      timeout_thread->request_stop();
      status_update_cv.notify_all();
      timeout_thread->join();
    }
  }

  void update(dfmessages::DataflowStatus s)
  {
    std::lock_guard<std::mutex> lock(status_update_mutex);
    status = s;
    status_updated.store(true);
    received_time = std::chrono::steady_clock::now();
    status_update_cv.notify_all();
  }

  void dataflow_status_timeout_proc(std::stop_token stoken)
  {
    while (!stoken.stop_requested()) {

      std::unique_lock<std::mutex> lock(status_update_mutex);
      status_update_cv.wait_for(lock, status_timeout_ms);
      if (std::chrono::steady_clock::now() - received_time >= status_timeout_ms) {
        status_updated.store(false);
      }
    }
  }
};

} // namespace dunedaq::dfmodules

#endif // DFMODULES_SRC_DFMODULES_RECEIVEDDATAFLOWSTATUS_HPP_
