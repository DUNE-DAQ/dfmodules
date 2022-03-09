#!/usr/bin/env python3

import argparse
import datetime
import h5py
import struct
from hdf5_dump import print_header, print_fragment_header


def get_record_locations_from_binary_file(fname):
    frag_magic_word = bytes.fromhex("22221111")
    trh_magic_word = bytes.fromhex("44443333")
    trh_loc = []
    frag_loc = []
    with open(fname, "rb") as f:
        iloc = 0
        nrec = 0
        while (byte := f.read(4)):
            if byte == trh_magic_word:
                trh_loc.append(iloc)
                frag_loc.append([])
                nrec += 1
            if byte == frag_magic_word:
                frag_loc[nrec-1].append(iloc)
            iloc += 4
    print(80*'=')
    print(19*'=', " Location of records in the binary file ", 19*'=')
    print(80*'=')
    for i in range(len(trh_loc)):
        if i >= g_n_request:
            break
        print("trigger {:<8} header - {:<16}".format(i+1, trh_loc[i]),
              "fragments: ", frag_loc[i])
    return (trh_loc, frag_loc)


def parse_binary_file(fname, k_n_request, k_print_out, k_list_components):
    frag_magic_word = bytes.fromhex("22221111")
    trh_magic_word = bytes.fromhex("44443333")
    tsl_magic_word = bytes.fromhex("66665555")
    with open(fname, "rb") as f:
        bytesbuffer = bytearray()
        ntrh = 0
        nfrag = 0
        record_type = ""
        while (byte := f.read(4)):
            if record_type == "":
                if byte == trh_magic_word:
                    record_type = "TriggerRecord"
                if byte == tsl_magic_word:
                    record_type = "TimeSlice"
            if byte == trh_magic_word or byte == tsl_magic_word:
                nfrag = 0
                if ntrh !=0 and k_print_out in ['both', 'fragment']:
                    print(80*'-')
                    print_fragment_header(bytesbuffer)
                bytesbuffer = bytearray()
                ntrh += 1
                if ntrh > k_n_request and k_n_request != 0: break
            if byte == frag_magic_word:
                if nfrag == 0 and ntrh != 0 and k_print_out in ['both',
                                                                'header']:
                    print(80*'=')
                    print_header(bytesbuffer, record_type, k_list_components)
                if nfrag != 0 and k_print_out in ['both', 'fragment']:
                    print(80*'-')
                    print_fragment_header(bytesbuffer)
                bytesbuffer = bytearray()
                nfrag += 1
            bytesbuffer.extend(byte)
    return


def parse_args():
    parser = argparse.ArgumentParser(
        description='Python script to parse binary files converted from HDF5.')

    parser.add_argument('-f', '--file-name',
                        help='path to binary file',
                        required=True)

    parser.add_argument('-p', '--print-out',
                        choices=['header', 'fragment', 'both'],
                        default='both',
                        help='select which part of data to display')

    parser.add_argument('-l', '--list-components',
                        help='''list components in trigger record header, used
                        with "--print-out header" or "--print-out both", not
                        applicable to TimeSlice data''', action='store_true')


    parser.add_argument('-d', '--debug',
                        help='print locations of headers in the file',
                        action='store_true')

    parser.add_argument('-n', '--num-of-records', type=int,
                        help='specify number of records to be parsed',
                        default=0)

    parser.add_argument('-v', '--version', action='version',
                        version='%(prog)s 1.0')
    return parser.parse_args()


def main():
    args = parse_args()

    bfile = args.file_name
    print("Reading file", bfile)

    parse_binary_file(bfile, args.num_of_records, args.print_out,
                      args.list_components)
    if args.debug:
        get_record_locations_from_binary_file(bfile)
    return


if __name__ == "__main__":
    main()
