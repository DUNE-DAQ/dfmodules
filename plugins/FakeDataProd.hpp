/**
 * @file FakeDataProd.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FAKEDATAPROD_HPP_
#define DFMODULES_PLUGINS_FAKEDATAPROD_HPP_

#include "dataformats/Fragment.hpp"
#include "dfmessages/DataRequest.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "appfwk/DAQSource.hpp"
#include "appfwk/ThreadHelper.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief FakeDataProd is simply an example
 */
class FakeDataProd : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FakeDataProd Constructor
   * @param name Instance name for this FakeDataProd instance
   */
  explicit FakeDataProd(const std::string& name);

  FakeDataProd(const FakeDataProd&) = delete;            ///< FakeDataProd is not copy-constructible
  FakeDataProd& operator=(const FakeDataProd&) = delete; ///< FakeDataProd is not copy-assignable
  FakeDataProd(FakeDataProd&&) = delete;                 ///< FakeDataProd is not move-constructible
  FakeDataProd& operator=(FakeDataProd&&) = delete;      ///< FakeDataProd is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);

  // Threading
  dunedaq::appfwk::ThreadHelper m_thread;
  void do_work(std::atomic<bool>&);

  // Configuration
  // size_t sleepMsecWhileRunning_;
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::dataformats::run_number_t m_run_number;
  uint32_t m_fake_link_number; // NOLINT

  // Queue(s)
  using datareqsource_t = dunedaq::appfwk::DAQSource<dfmessages::DataRequest>;
  std::unique_ptr<datareqsource_t> m_data_request_input_queue;
  using datafragsink_t = dunedaq::appfwk::DAQSink<std::unique_ptr<dataformats::Fragment>>;
  std::unique_ptr<datafragsink_t> m_data_fragment_output_queue;
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FAKEDATAPROD_HPP_
