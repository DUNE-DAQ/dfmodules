## Trigger Record Builder metrics

The trigger record builder, by construction, controls the flow of information as it both start a data loop and it closes it.
Because of that it's naturally suited to prompt a lot of information.
This page describes the metrics in details and outlines some typical situations and how they can be recognised by the metrics.

Metrics are grouped by logic.

### Status metrics

These are metrics that report instantaneous glimps of the status of the TRB. 
They are

+ ***pending trigger decisions***: it is the number of trigger decisions held in the TRB buffer waiting to be completed.
+ ***fragments in the book***: it is the number of fragments belonging to the pending tirgger decisions that have already been received.
+ ***pending fragments***: it is the difference between the number of expected fragments fromm the pending trigger decisions and the fragment in the book.

In normal conditions these metrics are usually 0. 
That is because the system complets TR contruction much faster than how the system probes the metrics. 
Yet, it's not an error if these values are not zero. 

### Error metrics

As the name suggests, these are metrics that monitor error conditions, to allow quick recognition of undesirable events without having to look at the logs. 
If these metrics are not zero, detailed errors shall be present in the logs. 
Due to the importance of these events, these metrics are resetted only at the start of a new run.
The list is

+ ***timed out trigger records***: depending on the configuration, the TRB builder can timout a TR creation. When that happens, an incomplete TR is send out. Although this is a desired behaviour, this is in a way data loss, that is why it is flagged as error.
+ ***lost fragments***: this is the number of fragments not received when a TR times out, that keeps accumulating. These fragments are classified as lost because even if they are simply late, when they received after its correpsonding TR is sent out, they are deleted and not sent to a writed module. 
+ ***unexpected fragments***: this identifies every fragments which are received wihtout a corresponding TR in he TRB buffer. It is considered an error condition since the missing TR means that the only possible solution is to delete the fragment, effectively causing data loss. It can happen that a fragments is both classied as lost and unexpected in case it is received after a TR timout. Anyway, not all lost fragments will be unexpected: in that case it signals a misconfiguration. Similarly, not all lost fragments are unexpected, if they are not received at all, they are just lost. 
+ ***invalid_requests***: this counts how many requests are created by the TRB and cannot be sent because the request GeoID is not configured in the queue map of the TRB. A Data request is not data, yet the hypothetical data cannot be retrieved and it causes indirectly data loss. 
+ ***duplicated trigger ids***: TR are indexed using unique combinations of `trigger number`, `run number` and `sequence number`. If different trigger decisions come in bearing the same identifier, the TR cannot be created even if the timestamp are different. In that case the trigger decision is dropped, again causing hypotetical data to be lost. 

In a well configured run, the most likely error condition is obtained when fragments are late, that case `lost fragments` = `unexpected fragments` != `0`. 
Yet, because of the time the metric are set, ***during***  the run this manifests with `unepxected fragments` < `lost fragments` since a fragments can be flagged as _lost_ as soon as its TR times out, while it can be flagged as _unexpected_ only when it is received.
Using only metrics, the proper understanding of what happened during the run, can only be determined once stop is called and, even then, assuming that the stop didn't prevent all the late fragments to be received and be properly flagged as _unexpected_. 
Of course the logs will flag the details of the situation during the run. 

### Operation monitoring metrics 

These are quantities evaluated over relatively short time intervals (seconds). Specifically they are calculated between the calls of `get_info()`. The list is
+ ***average_millisecond_per_trigger***
+ ***loop_counter***
+ ***sleep_counter***
