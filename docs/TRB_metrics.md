## Trigger Record Builder metrics

The trigger record builder, by construction, controls the flow of information as it both starts a data loop and it closes it.
Because of that it's naturally suited to prompt a lot of information.
This page describes the metrics in details and outlines some typical situations and how they can be recognised by the metrics.

Metrics are grouped in caterogies whose logic reflects in the different ways they are sampled. 
See the dedicated paragraphs for the details. 

### Status metrics

These are metrics that report instantaneous glimps of the status of the TRB. 
They are

+ ***pending trigger decisions***: it is the number of trigger decisions held in the TRB buffer waiting to be completed, meaning they are waiting for their requested fragments to arrive.
+ ***fragments in the book***: it is the number of fragments belonging to the pending trigger decisions that have already been received.
+ ***pending fragments***: it is the difference between the number of expected fragments fromm the pending trigger decisions and the fragment in the book.

In normal conditions these metrics are usually low. 
That is because the system completes TR contruction much faster than how the system probes the metrics. 
Yet, it's not an error if these values are bigger than zero, in fact it is expected to be so once we move from regular trigger into proper random requests.

### Error metrics

As the name suggests, these are metrics that monitor error conditions, to allow a quick recognition of undesirable events without having to look at the logs. 
If these metrics are not zero, detailed errors shall be present in the logs as well. 
Due to the importance of these events, these metrics are resetted only at the start of a new run and during the run they keep accumulating the error conditions.

The list is

+ ***timed out trigger records***: depending on the configuration, the TRB can timout a TR creation. When that happens, an incomplete TR is send out. Although this is a desired behaviour, this is in a way data loss since the missing fragments are not written into disk, that is why this condition is flagged as error.
+ ***lost fragments***: this is the number of fragments not received when a TR times out. These fragments are classified as lost because even if they are simply late, when they are received after its correpsonding TR is sent out, they are deleted and not sent to a writing module. 
+ ***unexpected fragments***: this identifies every fragment that is received without a corresponding TR in he TRB buffer. It is considered an error condition since the missing TR implies that the only possible solution is to delete the fragment, effectively causing data loss. It can happen that a fragments is both classied as lost and unexpected in case it is received after a TR timout. Anyway, not all lost fragments will be unexpected: in that case there has probably been a misconfiguration, or the fragments are coming from a previous run. Similarly, not all lost fragments are unexpected, if they are not received at all, they are just lost. 
+ ***unexpected trigger decisions***: this metric counts the number of trigger decisions that are received with a run number not associated with the current run number. These requests are simply deleted and no data requests are generated.
+ ***invalid requests***: this counts how many requests are created by the TRB and cannot be sent because the request SourceID is not configured in the queue map of the TRB. A data request is not data, yet without the request, the hypothetical data cannot be retrieved from readout and this indirectly causes data loss. 
+ ***duplicated trigger ids***: TR are indexed using unique combinations of `trigger number`, `run number` and `sequence number`. If different trigger decisions come in bearing the same identifier, the TR cannot be created even if the timestamp are different. In that case the trigger decision is dropped, again causing hypotetical data to be lost. Please note that keeping tracks of all the past TR decisions it's not efficient, so if a TR is send out and later another one with the same ID is received, it will not be discarded: this is still an error condition, but it will not be flagged by the TRB, not in metrics, nor in the logs.
+ ***abandoned trigger records***: once `stop` is called, the present TRs are sent to writing. In case the push is not possible because the queue is full, the system does not wait for the queue to be free as this would  delay the completition of the stop transition, so the TRs are deleted. If that happens this counter keeps track of this behaviour. The number of lost fragments is also increased as well according to the number of fragments contained in the deleted TR.

In a well configured run, the most likely error condition is obtained when fragments are late, and the signature is `lost fragments` = `unexpected fragments` != `0`. 
Yet, because of the time the metrics are set, ***during***  the run this manifests with `unepxected fragments` < `lost fragments` since a fragments can be flagged as _lost_ as soon as their TR times out, while fragements can only be flagged as _unexpected_ when they are received.
Using only metrics, the proper understanding of what happened during the run can only be determined once stop is called and, even then, assuming that the stop didn't prevent all the late fragments to be received and be properly flagged as _unexpected_. 
Of course the logs will flag the details of the situation during the run, without delay. 

### Operation monitoring metrics 

These are quantities evaluated over relatively short time intervals (seconds). 
Specifically they are calculated between the calls of `get_info()`. 
The list is

+ ***average millisecond per trigger***: this is the average time required for the TRs to be completed. The average is evaluated over the TRs completed in the time interval relative to the metric. If no TRs are completed, the time defaults to a negative number.
+ ***average data request width***: this is the average window width (in clock ticks) of the data requests generated by the TR. If no data requests are created, the time defaults to a negative number.
+ ***average decision width***: this is the averate width (in clock ticks) of the trigger decisions received by the TR. If no trigger decisions are received, the time defaults to a negative number. For a single trigger decision this is the smallest width that contains all the components of the trigger decisions. This metric, together with the average data request width, allows to monitor the correct creation of the requests. It also allows to monitor if decisions contain components with the same widths or not. Furthermore, if a maximum time readout window is set, this will monitor the slice operations. 
+ ***loop counter***: this counts the number of times that the loop performs operations on data during the time interval relative to metric.
+ ***sleep counter***: this counts the number of times that the loop goes to sleep for no new inputs are available from the input queues and therefore no changes in the internal status happened during a loop.

In normal conditions the average time per trigger is smaller than the TR timout. 
In non-busy conditions, that can go down to the sleep time set for the loop.

The sleep and loop counters are design to monitor how busy is the TRB. 
In tests performed so far, the sleep counter far outnumber the loop counter since the operations are trivial. 
Once the events size will grow, this might change.

### Run counters

These are counters that are increasing across the run and they are used to cross check if messages and data are correctly received between modules. 
For the TSB, the counters are:
***received trigger decisions***: the number of valid trigger decisions that are received during the run. Valid means that the correponding TR will be created and the corresponding data requests are sent. 
***generated trigger records***: the number of trigger records that are pushed into the output queue.
