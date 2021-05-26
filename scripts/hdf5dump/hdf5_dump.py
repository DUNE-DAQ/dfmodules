#!/usr/bin/env python3

# Version 1.0
# Last modified: April 10, 2021

# USAGE: python3 hdf5_dump.py -f sample.hdf5

import argparse
import datetime
import h5py
import struct

CLOCK_SPEED_HZ = 50000000.0

g_trigger_record_nfragments = []
g_trigger_record_nexp_fragments = []
g_trigger_record_number = []
g_ith_record = -1

g_n_request = 0
g_header_type = "both"
g_list_components = False
g_header_paths = []


def tick_to_timestamp(ticks):
    ns = float(ticks)/CLOCK_SPEED_HZ
    if ns < 3000000000:
        return datetime.datetime.fromtimestamp(ns)
    else:
        return "InvalidDateString"

def unpack_header(data_array, unpack_string, keys):
    values = struct.unpack(unpack_string, data_array)
    header = dict(zip(keys, values))
    return header


def get_geo_id_type(i):
    types = {1: 'TPC', 2: 'PDS', 3: 'DataSelection'}
    if i in types.keys():
        return types[i]
    else:
        return "Invalid"


def print_header_dict(hdict):
    filtered_list = ['Padding', 'GeoID version', 'Component request version']
    for ik, iv in hdict.items():
        if any(map(ik.__contains__, filtered_list)):
            continue
        elif "time" in ik or "begin" in ik or "end" in ik:
            print("{:<30}: {} ({})".format(
                ik, iv, tick_to_timestamp(iv)))
        elif 'Magic word' in ik:
            print("{:<30}: {}".format(ik, hex(iv)))
        elif 'GeoID type' in ik:
            print("{:<30}: {}".format(ik, get_geo_id_type(iv)))
        else:
            print("{:<30}: {}".format(ik, iv))
    return


def print_fragment_header(data_array):
    keys = ['Magic word', 'Version', 'Frag Size', 'Trig number',
            'Trig timestamp', 'Window begin', 'Window end', 'Run number',
            'Error bits', 'Fragment type', 'Fragment Padding',
            'GeoID version', 'GeoID type', 'GeoID region', 'GeoID element',
            'Geo ID Padding']
    unpack_string = '<2I5Q1I4I2H2I'
    print_header_dict(unpack_header(data_array[:80], unpack_string, keys))
    return


def print_trigger_record_header(data_array):
    keys = ['Magic word', 'Version', 'Trigger number',
            'Trigger timestamp', 'No. of requested components', 'Run Number',
            'Error bits', 'Trigger type']
    unpack_string = '<2I3Q2IH'
    print_header_dict(unpack_header(data_array[:42], unpack_string, keys))

    if g_list_components:
        comp_keys = ['Component request version', 'Component Request Padding',
                     'GeoID version', 'GeoID type', 'GeoID region',
                     'GeoID element', 'Geo ID Padding', 'Begin time',
                     'End time']
        comp_unpack_string = "<3I2H2I2Q"
        for i_values in struct.iter_unpack(comp_unpack_string,
                                           data_array[48:]):
            i_comp = dict(zip(comp_keys, i_values))
            print(80*'-')
            print_header_dict(i_comp)
    return


def process_record_func(name, dset):
    global g_ith_record
    global g_header_paths
    if g_n_request != 0 and g_ith_record >= g_n_request:
        return
    if isinstance(dset, h5py.Group) and "/" not in name:
        g_ith_record += 1
        g_header_paths.append({"TriggerRecordHeader": '', "Fragments": []})
        g_trigger_record_nfragments.append(0)
    if isinstance(dset, h5py.Dataset):
        if "TriggerRecordHeader" in name:
            g_header_paths[g_ith_record]["TriggerRecordHeader"] = name
            data_array = bytearray(dset[:])
            (i, j, k) = struct.unpack('<3Q', data_array[8:32])
            g_trigger_record_nexp_fragments.append(k)
            g_trigger_record_number.append(i)
        else:
            g_header_paths[g_ith_record]["Fragments"].append(name)
            g_trigger_record_nfragments[g_ith_record] += 1
    return


def process_file(file_name):
    global g_ith_record
    g_ith_record = -1
    with h5py.File(file_name, 'r') as f:
        f.visititems(process_record_func)
    return


def print_header(file_name):
    with h5py.File(file_name, 'r') as f:
        for i in g_header_paths:
            if i["TriggerRecordHeader"] == "":
                continue
            print(80*'=')
            if g_header_type in ["trigger", "both"]:
                dset = f[i["TriggerRecordHeader"]]
                data_array = bytearray(dset[:])
                print('{:<30}:\t{}'.format("Path", i["TriggerRecordHeader"]))
                print('{:<30}:\t{}'.format("Size", dset.shape))
                print('{:<30}:\t{}'.format("Data type", dset.dtype))
                print_trigger_record_header(data_array)
            if g_header_type in ["fragment", "both"]:
                for j in i["Fragments"]:
                    dset = f[j]
                    data_array = bytearray(dset[:])
                    print(80*'-')
                    print('{:<30}:\t{}'.format("Path", j))
                    print('{:<30}:\t{}'.format("Size", dset.shape))
                    print('{:<30}:\t{}'.format("Data type", dset.dtype))
                    print_fragment_header(data_array)
    return


def check_fragments():
    print("{:-^60}".format("Column Definitions"))
    print("i:           Trigger record number;")
    print("N_frag_exp:  expected no. of fragments stored in header;")
    print("N_frag_act:  no. of fragments written in trigger record;")
    print("N_diff:      N_frag_act - N_frag_exp")
    print("{:-^60}".format("Column Definitions"))
    print("{:^10}{:^15}{:^15}{:^10}".format(
        "i", "N_frag_exp", "N_frag_act", "N_diff"))
    for i in range(len(g_trigger_record_nexp_fragments)):
        print("{:^10}{:^15}{:^15}{:^10}".format(
            g_trigger_record_number[i], g_trigger_record_nexp_fragments[i],
            g_trigger_record_nfragments[i],
            (g_trigger_record_nfragments[i] -
             g_trigger_record_nexp_fragments[i])))
    return


def parse_args():
    parser = argparse.ArgumentParser(
        description='Python script to parse DUNE-DAQ HDF5 output files.')

    parser.add_argument('-f', '--file_name',
                        help='path to HDF5 file',
                        required=True)

    parser.add_argument('-p', '--print-headers',
                        choices=['trigger', 'fragment', 'both'],
                        help='select which header data to display')

    parser.add_argument('-c', '--check-fragments',
                        help='''check if fragments written in trigger record
                        matches expected number in trigger record header''',
                        action='store_true')

    parser.add_argument('-l', '--list-components',
                        help='''list components in trigger record header, used
                        together with "--print-headers trigger" or
                        "--print-headers both"''', action='store_true')

    parser.add_argument('-n', '--num-of-records', type=int,
                        help='specify number of trigger records to be parsed',
                        default=0)

    parser.add_argument('-v', '--version', action='version',
                        version='%(prog)s 1.0')
    return parser.parse_args()


def main():
    args = parse_args()

    global g_n_request
    global g_header_type
    global g_list_components

    g_n_request = args.num_of_records
    g_header_type = args.print_headers
    g_list_components = args.list_components

    if g_header_type is None and args.check_fragments is False:
        print("Error: use at least one of the two following options:")
        print("       -p, --print-headers {trigger,fragment,both}")
        print("       -c, --check-fragments")
        return

    h5file = args.file_name
    print("Reading file", h5file)

    process_file(h5file)
    if g_header_type is not None:
        print_header(h5file)
    if args.check_fragments:
        check_fragments()
    return


if __name__ == "__main__":
    main()
