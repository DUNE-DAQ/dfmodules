/**
 * @file FakeDataProd.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DDPDEMO_SRC_FAKEDATAPROD_HPP_
#define DDPDEMO_SRC_FAKEDATAPROD_HPP_

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
namespace ddpdemo {

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
  dunedaq::appfwk::ThreadHelper thread_;
  void do_work(std::atomic<bool>&);

  // Configuration
  // size_t sleepMsecWhileRunning_;
  std::chrono::milliseconds queueTimeout_;

  // Queue(s)
  using datareqsource_t = dunedaq::appfwk::DAQSource<dfmessages::DataRequest>;
  std::unique_ptr<datareqsource_t> dataRequestInputQueue_;
  using datafragsink_t = dunedaq::appfwk::DAQSink<std::unique_ptr<dataformats::Fragment>>;
  std::unique_ptr<datafragsink_t> dataFragmentOutputQueue_;
};
} // namespace ddpdemo
} // namespace dunedaq

#endif // DDPDEMO_SRC_FAKEDATAPROD_HPP_
