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
