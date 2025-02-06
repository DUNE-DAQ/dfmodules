# The DFO Protocol

## Background

The DFO process is responsible for accepting TriggerDecisions from the MLT and distributing them to the Dataflow applications for event-building and data writing. It uses messages from the Dataflow application to determine their current load, and if necessary, will issue an Inhibit message to the MLT to allow the Dataflow time to catch up.

At the 2024 Application Framework Review Workshop, the DFO was identified as a cricical single-point-of-failure in the DAQ system.

## The Old DFO Protocol

![image](DFO_Protocol_Old.png)

In the original design, the DFO received tokens from the Dataflow applications, which indicated that a TriggerRecord had been completed. The DFO would use the number of tokens received to calculate the busy status. These tokens, and TriggerDecision messages, were sent in one-to-one mode on the network.

## The New DFO Protocol

![image](DFO_Protocol_New_Simple.png)

The idea behind the new DFO protocol is that the control messages now are broadcasts (Pub/Sub) rather than one-to-one. The Dataflow application sends a periodic heartbeat message which contains information about the number of TriggerRecords currently being built and written.

![image](DFO_Protocol_New.png)

The advantage of this change is that now multiple DFOs can listen to the broadcast heartbeat and TriggerDecision messages, and create their own "DFODecision" messages in response to a complete view of the system. One DFO application is designated as the "active" DFO, and the Dataflow and MLT application ignore messages from inactive DFOs. Run control has an "enable_dfo" message, which when sent to these processes changes the DFO ID that they accept information from. (No change is needed on the DFO side, since they always operate as if they are active.) Heartbeat messages are used by non-active DFOs to update their view of the running system in case different decisions were reached (i.e. a TriggerRecord was assigned to a different Dataflow application by an inactive DFO than by the active DFO).

### DFO hand-off (enable_dfo command)

## Configuration Notes

Relative to previous (v5.2.x) configurations, there are minimal changes necessary. 

The following changes in `daqsystemtest/connections.data.xml`:
1. A DataflowHeartbeat kPubSub NetworkConnectionDescriptor should be added
1. A DFODecision kSendRecv NetworkConnectionDescriptor should be added
1. The type of the TriggerDecisions NetworkConnectionDescriptor should be changed from kSendRecv to kPubSub
1. The TriggerDecisionToken NetworkConnectionDescriptor can be removed
1. NetworkConnectionRules for DFODecisions and DataflowHeartbeats should be added
1. The TriggerDecisionToken NetworkConnectionRule should be removed
1. QueueDescriptor for TriggerDecisionToken should be added

The following changes in `daqsystemtest/df-segment.data.xml`:
1. Updated rule references
1. Added DFOBrokerConf module configuraiton, referenced in DataflowApplication instances

The following changes in `daqsystemtest/fsm.data.xml`:
1. Added enable-dfo FSMaction, FSMtransition, and FSMxTransition

With these configuration changes, and updates to DFApplication and DFOApplication in `appmodel`, the system configuration becomes:
![image](DFO_Protocol_session.png)

![image](DFO_Protocol_mlt.png)
The MLT application is unchanged, with the exception that it is now has a configuration parameter to set the "initally-active" DFO.

![image](DFO_Protocol_df.png)
Note how the DFOBroker now receives messages from the DataWriter and dispatches TriggerDecisions to the TRBModule. It is responsible for sending the periodic DataflowHeartbeat messages to the DFO.

![image](DFO_Protocol_dfo.png)
