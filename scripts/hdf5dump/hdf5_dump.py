#!/usr/bin/python3

# Version 1.0
# Last modified: April 10, 2021

# USAGE: python3 hdf5_dump.py -f sample.hdf5

import argparse

import h5py
import datetime

import struct

CLOCK_SPEED_HZ = 50000000.0

g_trigger_record_nfragments = []
g_trigger_record_nexp_fragments = []
g_ith_record = -1

g_n_printed = 0
g_n_request = 0
g_header_type = "both"
g_list_components = False


def tick_to_timestamp(ticks):
    ns = float(ticks)/CLOCK_SPEED_HZ
    return datetime.datetime.fromtimestamp(ns)


def unpack_header(data_array, unpack_string, keys):
    values = struct.unpack(unpack_string, data_array)
    header = dict(zip(keys, values))
    return header


def print_header(hdict):
    for ik, iv in hdict.items():
        if "timestamp" in ik:
            print("{:<30}: {} ({})".format(
                ik, iv, tick_to_timestamp(iv)))
        elif 'Magic word' in ik:
            print("{:<30}: {}".format(ik, hex(iv)))
        else:
            print("{:<30}: {}".format(ik, iv))
    return


def print_fragment_header(data_array):
    keys = ['Magic word', 'Version', 'Frag Size', 'Trig number',
            'Trig timestamp', 'Window begin (ticks@50MHz)',
            'Window end (ticks@50MHz)', 'Run number',
            'GeoID (APA)', 'GeoID (link)', 'Error bits', 'Fragment type']
    unpack_string = '<2I5Q5I'
    print_header(unpack_header(data_array[:68], unpack_string, keys))
    return


def print_trigger_record_header(data_array):
    keys = ['Magic word', 'Version', 'Trigger number',
            'Trigger timestamp', 'No. of requested components', 'Run Number',
            'Error bits', 'Trigger type']
    unpack_string = '<2I3Q2IH'
    print_header(unpack_header(data_array[:42], unpack_string, keys))

    if g_list_components:
        comp_keys = ['GeoID (APA)', 'GeoID (link)', 'Begin time (ticks@50MHz)',
                     'End time (ticks@50MHz)']
        comp_unpack_string = "<2I2Q"
        for i_values in struct.iter_unpack(comp_unpack_string,
                                           data_array[48:]):
            i_comp = dict(zip(comp_keys, i_values))
            print(80*'-')
            print_header(i_comp)
    return


def get_header_func(name, dset):
    global g_n_printed
    if isinstance(dset, h5py.Dataset):
        if g_n_request <= 0 or g_n_printed < g_n_request:
            if "TriggerRecordHeader" in name:
                if g_header_type in ['trigger', 'both']:
                    g_n_printed += 1
                    data_array = bytearray(dset[:])
                    print(80*'=')
                    print('{:<30}:\t{}'.format("Path", name))
                    print('{:<30}:\t{}'.format("Size", dset.shape))
                    print('{:<30}:\t{}'.format("Data type", dset.dtype))
                    print_trigger_record_header(data_array)
            else:
                if g_header_type in ['fragment', 'both']:
                    g_n_printed += 1
                    data_array = bytearray(dset[:])
                    print(80*'=')
                    print('{:<30}:\t{}'.format("Path", name))
                    print('{:<30}:\t{}'.format("Size", dset.shape))
                    print('{:<30}:\t{}'.format("Data type", dset.dtype))
                    print_fragment_header(data_array)
    return


def get_header(file_name):
    with h5py.File(file_name, 'r') as f:
        f.visititems(get_header_func)
    return


def examine_fragments_func(name, dset):
    global g_ith_record
    if isinstance(dset, h5py.Group) and "/" not in name:
        # This is a new trigger record.
        g_trigger_record_nfragments.append(0)
        g_ith_record += 1
    if isinstance(dset, h5py.Dataset):
        if "FELIX" in name:
            # This is a new fragment
            g_trigger_record_nfragments[g_ith_record] += 1
        if "TriggerRecordHeader" in name:
            # This is a new TriggerRecordHeader
            data_array = bytearray(dset[:])
            (i,) = struct.unpack('<Q', data_array[24:32])
            g_trigger_record_nexp_fragments.append(i)
    return


def examine_fragments(file_name):
    with h5py.File(file_name, 'r') as f:
        f.visititems(examine_fragments_func)

    print("{:-^60}".format("Column Definitions"))
    print("i:           Trigger record number;")
    print("N_frag_exp:  expected no. of fragments stored in header;")
    print("N_frag_act:  no. of fragments written in trigger record;")
    print("N_diff:      N_frag_act - N_frag_exp")
    print("{:-^60}".format("Column Definitions"))
    print("{:^10}{:^15}{:^15}{:^10}".format(
        "i", "N_frag_exp", "N_frag_act", "N_diff"))
    for i in range(len(g_trigger_record_nfragments)):
        print("{:^10}{:^15}{:^15}{:^10}".format(
            i, g_trigger_record_nexp_fragments[i],
            g_trigger_record_nfragments[i],
            (g_trigger_record_nfragments[i] -
             g_trigger_record_nexp_fragments[i])))
    return


def parse_args():
    parser = argparse.ArgumentParser(
        description='Python script to parse DUNE-DAQ HDF5 output files.')

    parser.add_argument('-f', '--file_name',
                        help='Path to HDF5 file',
                        required=True)

    parser.add_argument('--header', choices=['trigger', 'fragment', 'both'],
                        default='both',
                        help='Select which header data to display')

    parser.add_argument('--check-fragments',
                        help='''check if fragments written in trigger record
                        matches expected number in trigger record header''',
                        action='store_true')

    parser.add_argument('--list-components',
                        help='list components in trigger record header',
                        action='store_true')

    parser.add_argument('-n', '--num-of-records', type=int,
                        help='Select number of records to be parsed',
                        default=0)

    parser.add_argument('-v', '--version', action='version',
                        version='%(prog)s 1.0')
    return parser.parse_args()


def main():
    args = parse_args()

    h5file = args.file_name
    print("Reading file", h5file)

    global g_n_request
    global g_header_type
    global g_list_components

    g_n_request = args.num_of_records
    g_header_type = args.header
    g_list_components = args.list_components

    if not args.check_fragments:
        get_header(h5file)
    else:
        examine_fragments(h5file)


if __name__ == "__main__":
    main()
