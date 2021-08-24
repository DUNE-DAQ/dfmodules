import h5py

class DataFile:
    def __init__(self, filename):
        self.h5file=h5py.File(filename, 'r')
        self.events=self.h5file.keys()
        self.n_events=len(self.events)
    
def sanity_check(datafile):
    "Very basic sanity checks on file"
    passed=True
    # Check that we didn't miss any events (sort of)
    #
    # This condition isn't true if there are multiple files for this
    # run, and this file isn't the first one, so don't do it
    # assert list(datafile.events)[-1] == ("TriggerRecord%05d" % datafile.n_events)
    # Check that every event has a TriggerRecordHeader
    for event in datafile.events:
        if "TriggerRecordHeader" not in datafile.h5file[event].keys():
            print(f"No TriggerRecordHeader in event {event}")
            passed=False
    if passed:
        print("Sanity-check passed")
    return passed

def check_event_count(datafile, min_event_count, max_event_count):
    "Check that the number of events is between min_event_count and max_event_count"
    passed=True
    event_count=len(datafile.events)
    if event_count<min_event_count or event_count>max_event_count:
        passed=False
        print(f"Event count {event_count} is outside range [{min_event_count}, {max_event_count}]")
    if passed:
        print(f"Event count is between {min_event_count} and {max_event_count}")
    return passed

def check_link_presence(datafile, n_links):
    "Check that there are n_links links in each event in file"
    passed=True
    for event in datafile.events:
        for i in range(n_links):
            link_name=("Link%02d" % i)
            if link_name not in datafile.h5file[event]["TPC/APA000"].keys():
                passed=False
                print(f"Link {link_name} not present in event {event}")
    if passed:
        print(f"{n_links} links present in all {datafile.n_events} events")
    return passed

# 18-Aug-2021, KAB: General-purposed test for fragment count.  The idea behind this
# test is that each type of fragment is tested individually, by calling this routine for
# each type.  The test is driven by a set of parameters that describe both the fragments
# to be tested (e.g. the HDF5 Group names) and the characteristics that they should have
# (e.g. the number of fragments that should be present).
#
# The parameters that are required by the 'check_fragment_presence' routine are the following:
# * fragment_type_description - descriptive text for the fragment type, e.g. "WIB" or "PDS" or "Raw TP"
# * hdf5_groupss - the two HDF5 Groups in the DataSet path between the TriggerRecord identifier and
#                 the DataSet name, e.g. "TPC/APA000" or "PDS/Region000"
# * element_name_prefix - the prefix that is used to describe the detector element in the HDF5 DataSet
#                         name, e.g. "Link" or "Element"
# * element_number_offset - the offset that should be used for the element numbers for the fragments,
#                           typically, this is zero, but it is currently non-zero for Raw_TP fragments
# * expected_fragment_count - the expected number of fragments of this type
def check_fragment_count(datafile, params):
    "Checking that there are {params['expected_fragment_count']} {params['fragment_type_description']} fragments in each event in file"
    passed=True
    for event in datafile.events:
        fragments_present=len(datafile.h5file[event][params['hdf5_groups']].keys())
        if fragments_present != params['expected_fragment_count']:
            passed=False
            print(f"Event {event} has an unexpected number of {params['fragment_type_description']} fragments: {fragments_present} (expected {params['expected_fragment_count']})")
    if passed:
        print(f"{params['fragment_type_description']} fragment count of {params['expected_fragment_count']} confirmed in all {datafile.n_events} events")
    return passed

# 18-Aug-2021, KAB: General-purposed test for fragment presence.  The idea behind this
# test is that each type of fragment is tested individually, by calling this routine for
# each type.  The test is driven by a set of parameters that describe both the fragments
# to be tested (e.g. the HDF5 Group names) and the characteristics that they should have
# (e.g. the number of fragments that should be present).
#
# The parameters that are required by the 'check_fragment_presence' routine are the following:
# * fragment_type_description - descriptive text for the fragment type, e.g. "WIB" or "PDS" or "Raw TP"
# * hdf5_groupss - the two HDF5 Groups in the DataSet path between the TriggerRecord identifier and
#                 the DataSet name, e.g. "TPC/APA000" or "PDS/Region000"
# * element_name_prefix - the prefix that is used to describe the detector element in the HDF5 DataSet
#                         name, e.g. "Link" or "Element"
# * element_number_offset - the offset that should be used for the element numbers for the fragments,
#                           typically, this is zero, but it is currently non-zero for Raw_TP fragments
# * expected_fragment_count - the expected number of fragments of this type
def check_fragment_presence(datafile, params):
    "Checking that there are {params['expected_fragment_count']} {params['fragment_type_description']} fragments in each event in file"
    passed=True
    for event in datafile.events:
        for i in range(params['expected_fragment_count']):
            dataset_name=(params['element_name_prefix'] + "%02d" % (i + params['element_number_offset']))
            if dataset_name not in datafile.h5file[event][params['hdf5_groups']].keys():
                passed=False
                print(f"{params['fragment_type_description']} fragment for {dataset_name} not present in event {event}")
    if passed:
        print(f"{params['expected_fragment_count']} {params['fragment_type_description']} fragments present in all {datafile.n_events} events")
    return passed

def check_fragment_sizes(datafile, min_frag_size, max_frag_size):
    "Check that every fragment is between min_frag_size and max_frag_size"
    passed=True
    for event in datafile.events:
        for link in datafile.h5file[event]["TPC/APA000"].values():
            size=link.shape[0]
            if size<min_frag_size or size>max_frag_size:
                passed=False
                print(f"Link {link} in event {event} has size {size}, outside range [{min_frag_size}, {max_frag_size}]")
    if passed:
        print(f"All links in {datafile.n_events} events have fragment sizes between {min_frag_size} and {max_frag_size}")
    return passed

# 18-Aug-2021, KAB: general-purposed test for fragment sizes.  The idea behind this
# test is that each type of fragment is tested individually, by calling this routine for
# each type.  The test is driven by a set of parameters that describe both the fragments
# to be tested (e.g. the HDF5 Group names) and the characteristics that they should have
# (e.g. the minimum and maximum fragment size).
#
# The parameters that are required by the 'check_fragment_presence' routine are the following:
# * fragment_type_description - descriptive text for the fragment type, e.g. "WIB" or "PDS" or "Raw TP"
# * hdf5_groupss - the two HDF5 Groups in the DataSet path between the TriggerRecord identifier and
#                 the DataSet name, e.g. "TPC/APA000" or "PDS/Region000"
# * element_name_prefix - the prefix that is used to describe the detector element in the HDF5 DataSet
#                         name, e.g. "Link" or "Element"
# * element_number_offset - the offset that should be used for the element numbers for the fragments,
#                           typically, this is zero, but it is currently non-zero for Raw_TP fragments
# * min_size_bytes - the minimum size of fragments of this type
# * max_size_bytes - the maximum size of fragments of this type
def check_fragment_size2(datafile, params):
    "Check that every {params['fragment_type_description']} fragment size is between {params['min_size_bytes']} and {params['max_size_bytes']}"
    passed=True
    for event in datafile.events:
        for frag in datafile.h5file[event][params['hdf5_groups']].values():
            size=frag.shape[0]
            if size<params['min_size_bytes'] or size>params['max_size_bytes']:
                passed=False
                print(f" {params['fragment_type_description']} fragment {frag.name} in event {event} has size {size}, outside range [{params['min_size_bytes']}, {params['max_size_bytes']}]")
    if passed:
        print(f"All {params['fragment_type_description']} fragments in {datafile.n_events} events have sizes between {params['min_size_bytes']} and {params['max_size_bytes']}")
    return passed
