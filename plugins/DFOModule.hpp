/**
 * @file DFOModule.hpp
 *
 * This is part of the DUNE DAQ Software Suite, copyright 2020.
 * Licensing/copyright details are in the COPYING file that you should have
 * received with this code.
 */

#ifndef DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
#define DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_

#include "dfmodules/DFOCore.hpp"

#include "appmodel/DFOConf.hpp"
#include "dfmessages/TriggerDecisionToken.hpp"
#include "dfmessages/TriggerInhibit.hpp"
#include "iomanager/Sender.hpp"

#include "appfwk/DAQModule.hpp"
#include "logging/Logging.hpp"

#include <memory>
#include <string>
#include <vector>

namespace dunedaq {
namespace dfmodules {

/**
 * @brief DFOModule distributes triggers according to the
 * availability of the DF apps in the system.
 */
class DFOModule : public dunedaq::appfwk::DAQModule
{
public:
  /**
   * @brief DFOModule Constructor
   * @param name Instance name for this DFOModule instance
   */
  explicit DFOModule(const std::string& name);

  DFOModule(const DFOModule&) = delete;            ///< DFOModule is not copy-constructible
  DFOModule& operator=(const DFOModule&) = delete; ///< DFOModule is not copy-assignable
  DFOModule(DFOModule&&) = delete;                 ///< DFOModule is not move-constructible
  DFOModule& operator=(DFOModule&&) = delete;      ///< DFOModule is not move-assignable

  void init(std::shared_ptr<appfwk::ConfigurationManager> mcfg) override;

private:
  std::unique_ptr<DFOCore> m_core;
  // Commands
  void do_conf(const CommandData_t&);
  void do_start(const CommandData_t&);
  void do_stop(const CommandData_t&);
  void do_scrap(const CommandData_t&);

  void generate_opmon_data() override;

  // Configuration
  const appmodel::DFOConf* m_dfo_conf{ nullptr };

  // Connections (initialised in init(), passed to core at start())
  std::shared_ptr<iomanager::SenderConcept<dfmessages::TriggerInhibit>> m_busy_sender;
  std::string m_token_connection;
  std::string m_td_connection;
  std::vector<std::string> m_trb_conn_ids;
};

} // namespace dfmodules
} // namespace dunedaq

#endif // DFMODULES_PLUGINS_DATAFLOWORCHESTRATOR_HPP_
