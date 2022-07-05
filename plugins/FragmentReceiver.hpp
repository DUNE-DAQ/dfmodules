/**
 * @file FragmentReceiver.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_FRAGMENTRECEIVER_HPP_
#define DFMODULES_PLUGINS_FRAGMENTRECEIVER_HPP_

#include "daqdataformats/Fragment.hpp"

#include "appfwk/DAQModule.hpp"
#include "iomanager/ConnectionId.hpp"
#include "iomanager/Sender.hpp"

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

  using internal_data_t = std::unique_ptr<daqdataformats::Fragment>;

  void dispatch_fragment(internal_data_t&);

  // Configuration
  std::chrono::milliseconds m_queue_timeout;
  dunedaq::daqdataformats::run_number_t m_run_number;

  // Connections
  iomanager::connection::ConnectionRef m_input_connection;
  using fragmentsender_t = iomanager::SenderConcept<internal_data_t>;
  std::shared_ptr<fragmentsender_t> m_fragment_output;

  size_t m_received_fragments{ 0 };
};
} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_FRAGMENTRECEIVER_HPP_
