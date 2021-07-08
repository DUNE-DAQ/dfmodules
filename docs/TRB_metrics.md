## Trigger Record Builder metrics

The trigger record builder, by construction, controls the flow of information as it both start a data loop and it closes it.
Because of that it's naturally suited to prompt a lot of information.
This page describes the metrics in details and outlines some typical situations and how they can be recognised by the metrics.

Metrics are grouped by logic.

### Status metrics

These are metrics that report instantaneous glimps of the status of the TRB. They are
+ ***pending_trigger_decisions*** 
+ ***fragments_in_the_book***
+ ***pending_fragments***


### Error metrics

As the name suggests, these are metrics that monitor error conditions, to allow quick recognition of undesirable events without having to look at the logs. 
If these metrics are not zero, detailed errors shall be present in the logs. 
Due to the importance of these events, these metrics are resetted only at the start of a new run.
The list is
+ ***timed_out_trigger_records***
+ ***lost_fragments***
+ ***unexpected_fragments***
+ ***invalid_requests***
+ ***duplicated_trigger_ids***


### Operation monitoring metrics 

These are quantities evaluated over relatively short time intervals (seconds). Specifically they are calculated between the calls of `get_info()`. The list is
+ ***average_millisecond_per_trigger***
+ ***loop_counter***
+ ***sleep_counter***
