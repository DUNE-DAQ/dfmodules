#!/usr/bin/env python3

# Version 1.0
# Last modified: Feb 18, 2022

# USAGE: python3 hdf5_to_binary.py -i sample.hdf5 -o sample.bin [-n N]  [-r]

import argparse
import h5py
import os


# Global variables are required to save the save of the state of
# function call "h5py.File.visititems(process_record_func)"

g_ith_record = -1
g_header_paths = []


def process_record_func(name, dset):
    global g_ith_record
    global g_header_paths
    if isinstance(dset, h5py.Group) and "/" not in name:
        g_ith_record += 1
        g_header_paths.append({"TriggerRecordHeader": '', "Fragments": []})
    if isinstance(dset, h5py.Dataset):
        if "TriggerRecordHeader" in name:
            g_header_paths[g_ith_record]["TriggerRecordHeader"] = name
        else:
            g_header_paths[g_ith_record]["Fragments"].append(name)
    return


def process_hdf5_file(file_name):
    global g_ith_record
    g_ith_record = -1
    with h5py.File(file_name, 'r') as f:
        f.visititems(process_record_func)
    return


##
# g_header_paths is a list of maps
# [
#   {
#     'TriggerRecordHeader': 'TriggerRecord00000/TriggerRecordHeader',
#     'Fragments': ['TriggerRecord00000/FELIX/APA000/Link00',
#                   'TriggerRecord00000/FELIX/APA000/Link01']
#   }
# ]

def convert_to_binary_file(h5file, binary_file, n_record=-1):
    processed_record = 0
    process_hdf5_file(h5file)
    with open(binary_file, 'wb') as bf:
        with h5py.File(h5file, 'r') as hf:
            for i in g_header_paths:
                if processed_record >= n_record and n_record != -1:
                    continue
                if i["TriggerRecordHeader"] == "":
                    continue
                dset = hf[i["TriggerRecordHeader"]]
                idata_array = bytearray(dset[:])
                bf.write(idata_array)
                for j in i["Fragments"]:
                    dset = hf[j]
                    jdata_array = bytearray(dset[:])
                    bf.write(jdata_array)
                processed_record += 1
    return


def parse_args():
    parser = argparse.ArgumentParser(
        description='Python script to parse DUNE-DAQ HDF5 output files.')

    parser.add_argument('-i', '--input-file',
                        help='path to the input HDF5 file',
                        required=True)

    parser.add_argument('-o', '--output-file',
                        help='path to the output binary file',
                        required=True)

    parser.add_argument('-r', '--rewrite-file',
                        help='overwrite output binary file with the same name',
                        action='store_true')

    parser.add_argument('-n', '--num-of-records', type=int,
                        help='specify number of trigger records, -1 for all',
                        default=-1)

    parser.add_argument('-v', '--version', action='version',
                        version='%(prog)s 1.0')
    return parser.parse_args()


def main():
    args = parse_args()

    if os.path.isfile(args.output_file) and not args.rewrite_file:
        print("Error: Output binary file {} exists! ".format(args.output_file))
        print("Error: Use a different name or turn on '-r' option.")
        return
    convert_to_binary_file(args.input_file, args.output_file,
                           args.num_of_records)
    return


if __name__ == "__main__":
    main()
