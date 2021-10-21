/**
 * @file FragmentReceiver.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_
#define DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_

#include "dataformats/Fragment.hpp"

#include "appfwk/DAQModule.hpp"
#include "appfwk/DAQSink.hpp"
#include "ipm/Receiver.hpp"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief FragmentReceiver receives fragments then dispatches them to the appropriate queue
 */
class FragmentReceiver : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief FragmentReceiver Constructor
   * @param name Instance name for this FragmentReceiver instance
   */
  explicit FragmentReceiver(const std::string& name);

  FragmentReceiver(const FragmentReceiver&) = delete;            ///< FragmentReceiver is not copy-constructible
  FragmentReceiver& operator=(const FragmentReceiver&) = delete; ///< FragmentReceiver is not copy-assignable
  FragmentReceiver(FragmentReceiver&&) = delete;                 ///< FragmentReceiver is not move-constructible
  FragmentReceiver& operator=(FragmentReceiver&&) = delete;      ///< FragmentReceiver is not move-assignable

  void init(const data_t&) override;

private:
  // Commands
  void do_conf(const data_t&);
  void do_start(const data_t&);
  void do_stop(const data_t&);
  void do_scrap(const data_t&);

  void get_info(opmonlib::InfoCollector& ci, int level) override;

  void dispatch_fragment(ipm::Receiver::Response message);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::dataformats::run_number_t m_run_number;
  std::string m_connection_name;

  // Queue(s)
  using fragmentsink_t = dunedaq::appfwk::DAQSink<std::unique_ptr<dataformats::Fragment>>;
  std::unique_ptr<fragmentsink_t> m_fragment_output_queue;

  size_t m_received_fragments{ 0 };
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_REQUESTRECEIVER_HPP_
