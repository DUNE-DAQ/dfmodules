import datetime
import h5py
import os.path
import re

class DataFile:
    def __init__(self, filename):
        self.h5file=h5py.File(filename, 'r')
        self.events=self.h5file.keys()
        self.n_events=len(self.events)

def find_fragments_of_specified_type(grp, subsystem='', fragment_type=''):
    frag_list = [] # Local variable here
    def visitor(name, obj):
        nonlocal frag_list  # non-local to the visitor function
        pattern = ".*"
        if subsystem != '':
            pattern = f'.*/{subsystem}_0x[0-9a-fA-F]+_'
        if fragment_type != '':
            pattern += f'{fragment_type}'
        else:
            pattern += ".*"
        if isinstance(obj, h5py.Dataset):
            #print(f"checking {obj.name} against pattern {pattern}")
            if re.match(pattern, obj.name):
                frag_list.append(obj)
    grp["RawData"].visititems(visitor)
    #print(f"find_fragments_of_specified_type returning {len(frag_list)} fragments")
    return frag_list

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
        triggerrecordheader_count = 0
        for key in datafile.h5file[event]["RawData"].keys():
            if "TriggerRecordHeader" in key:
                triggerrecordheader_count += 1
        if triggerrecordheader_count == 0:
            print(f"No TriggerRecordHeader in event {event}")
            passed=False
        if triggerrecordheader_count > 1:
            print(f"More than one TriggerRecordHeader in event {event}")
            passed=False
    if passed:
        print("Sanity-check passed")
    return passed

def check_file_attributes(datafile):
    "Check that the expected Attributes exist within the data file"
    passed=True
    base_filename = os.path.basename(datafile.h5file.filename)
    expected_attribute_names = ["application_name", "closing_timestamp", "creation_timestamp", "file_index", "filelayout_params", "filelayout_version", "operational_environment", "recorded_size", "run_number"]
    for expected_attr_name in expected_attribute_names:
        if expected_attr_name not in datafile.h5file.attrs.keys():
            passed=False
            print(f"Attribute '{expected_attr_name}' not found in file {base_filename}")
        elif expected_attr_name == "run_number":
            # value from the Attribute
            attr_value = datafile.h5file.attrs.get(expected_attr_name)
            # value from the filename
            pattern = r"_run\d+_"
            match_obj = re.search(pattern, base_filename)
            if match_obj:
                filename_value = int(re.sub('run','',re.sub('_','',match_obj.group(0))))
                if attr_value != filename_value:
                    passed=False
                    print(f"The value in Attribute '{expected_attr_name}' ({attr_value}) does not match the value in the filename ({base_filename})")
        elif expected_attr_name == "file_index":
            # value from the Attribute
            attr_value = datafile.h5file.attrs.get(expected_attr_name)
            # value from the filename
            pattern = r"_\d+_"
            match_obj = re.search(pattern, base_filename)
            if match_obj:
                filename_value = int(re.sub('_','',match_obj.group(0)))
                if attr_value != filename_value:
                    passed=False
                    print(f"The value in Attribute '{expected_attr_name}' ({attr_value}) does not match the value in the filename ({base_filename})")
        elif expected_attr_name == "creation_timestamp":
            attr_value = datafile.h5file.attrs.get(expected_attr_name)
            date_obj = datetime.datetime.fromtimestamp((int(attr_value)/1000)-1, datetime.timezone.utc)
            date_string = date_obj.strftime("%Y%m%dT%H%M%S")
            pattern_low = f".*{date_string}.*"
            date_obj = datetime.datetime.fromtimestamp((int(attr_value)/1000)+1, datetime.timezone.utc)
            date_string = date_obj.strftime("%Y%m%dT%H%M%S")
            pattern_high = f".*{date_string}.*"
            date_obj = datetime.datetime.fromtimestamp((int(attr_value)/1000)+0, datetime.timezone.utc)
            date_string = date_obj.strftime("%Y%m%dT%H%M%S")
            pattern_exact = f".*{date_string}.*"
            if not re.match(pattern_exact, base_filename) and not re.match(pattern_low, base_filename) and not re.match(pattern_high, base_filename):
                passed=False
                print(f"The value in Attribute '{expected_attr_name}' ({date_string}) does not match the value in the filename ({base_filename})")
                print(f"Debug information: pattern_low={pattern_low} pattern_high={pattern_high} pattern_exact={pattern_exact}")
    if passed:
        print(f"All Attribute tests passed for file {base_filename}")
    return passed

def check_event_count(datafile, expected_value, tolerance):
    "Check that the number of events is within tolerance of expected_value"
    passed=True
    event_count=len(datafile.events)
    min_event_count=expected_value-tolerance
    max_event_count=expected_value+tolerance
    if event_count<min_event_count or event_count>max_event_count:
        passed=False
        print(f"Event count {event_count} is outside the tolerance of {tolerance} from an expected value of {expected_value}")
    if passed:
        print(f"Event count is within a tolerance of {tolerance} from an expected value of {expected_value}")
    return passed

# 18-Aug-2021, KAB: General-purposed test for fragment count.  The idea behind this test
# is that each type of fragment can be tested individually, by calling this routine for
# each type.  The test is driven by a set of parameters that describe both the fragments
# to be tested (e.g. the HDF5 Group names) and the characteristics that they should have
# (e.g. the number of fragments that should be present).
#
# The parameters that are required by this routine are the following:
# * fragment_type_description - descriptive text for the fragment type, e.g. "WIB" or "PDS" or "Raw TP"
# * fragment_type - Type of the Fragment, e.g. "ProtoWIB" or "Trigger_Primitive"
# * hdf5_source_subsystem - the Subsystem of the Fragments to find,
#                         e.g. "Detector_Readout" or "Trigger"
# * expected_fragment_count - the expected number of fragments of this type
def check_fragment_count(datafile, params):
    "Checking that there are {params['expected_fragment_count']} {params['fragment_type_description']} fragments in each event in file"
    passed=True
    for event in datafile.events:
        frag_list = find_fragments_of_specified_type(datafile.h5file[event], params['hdf5_source_subsystem'],
                                                     params['fragment_type']);
        fragment_count=len(frag_list)
        if fragment_count != params['expected_fragment_count']:
            passed=False
            print(f"Event {event} has an unexpected number of {params['fragment_type_description']} fragments: {fragment_count} (expected {params['expected_fragment_count']})")
    if passed:
        print(f"{params['fragment_type_description']} fragment count of {params['expected_fragment_count']} confirmed in all {datafile.n_events} events")
    return passed

# 18-Aug-2021, KAB: general-purposed test for fragment sizes.  The idea behind this test
# is that each type of fragment can be tested individually, by calling this routine for
# each type.  The test is driven by a set of parameters that describe both the fragments
# to be tested (e.g. the HDF5 Group names) and the characteristics that they should have
# (e.g. the minimum and maximum fragment size).
#
# The parameters that are required by this routine are the following:
# * fragment_type_description - descriptive text for the fragment type, e.g. "WIB" or "PDS" or "Raw TP"
# * fragment_type - Type of the Fragment, e.g. "ProtoWIB" or "Trigger_Primitive"
# * hdf5_source_subsystem - the Subsystem of the Fragments to find,
#                         e.g. "Detector_Readout" or "Trigger"
# * min_size_bytes - the minimum size of fragments of this type
# * max_size_bytes - the maximum size of fragments of this type
def check_fragment_sizes(datafile, params):
    "Check that every {params['fragment_type_description']} fragment size is between {params['min_size_bytes']} and {params['max_size_bytes']}"
    passed=True
    for event in datafile.events:
        frag_list = find_fragments_of_specified_type(datafile.h5file[event], params['hdf5_source_subsystem'],
                                                     params['fragment_type']);
        for frag in frag_list:
            size=frag.shape[0]
            if size<params['min_size_bytes'] or size>params['max_size_bytes']:
                passed=False
                print(f" {params['fragment_type_description']} fragment {frag.name} in event {event} has size {size}, outside range [{params['min_size_bytes']}, {params['max_size_bytes']}]")
    if passed:
        print(f"All {params['fragment_type_description']} fragments in {datafile.n_events} events have sizes between {params['min_size_bytes']} and {params['max_size_bytes']}")
    return passed


# ###########################################################################
# 11-Nov-2021, KAB: the following routines are deprecated.
# Please do not use them. If one of the routines defined above does not meet
# a need, let's talk about how to meet that need without using one of the
# routines defined below.
# ###########################################################################

def check_link_presence(datafile, n_links):
    "Check that there are n_links links in each event in file"
    passed=False
    print("The check_link_presence test has been deprecated. Please use check_fragment_count instead.")
    return passed

def check_fragment_presence(datafile, params):
    "Checking that there are the expected fragments in each event in file"
    passed=False
    print("The check_fragment_presence test has been deprecated. Please use check_fragment_count instead.")
    return passed
