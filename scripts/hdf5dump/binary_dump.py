#!/usr/bin/env python3

import argparse
import datetime
import h5py
import struct
from hdf5_dump import print_fragment_header, print_trigger_record_header

CLOCK_SPEED_HZ = 50000000.0

g_n_request = 0
g_header_type = "both"
g_list_components = False


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


def read_binary_file(fname):
    frag_magic_word = bytes.fromhex("22221111")
    trh_magic_word = bytes.fromhex("44443333")
    with open(fname, "rb") as f:
        bytesbuffer = bytearray()
        ntrh = 0
        nfrag = 0
        while (byte := f.read(4)):
            #print(byte)
            #if iloc > 4: break
            if byte == trh_magic_word:
                nfrag = 0
                if ntrh !=0 and g_header_type in ['both', 'fragment']:
                    print(80*'-')
                    print_fragment_header(bytesbuffer)
                bytesbuffer = bytearray()
                ntrh += 1
                if ntrh > g_n_request and g_n_request != 0: break
            if byte == frag_magic_word:
                if nfrag == 0 and ntrh != 0 and g_header_type in ['both',
                                                                  'trigger']:
                    print(80*'=')
                    print_trigger_record_header(bytesbuffer)
                if nfrag != 0 and g_header_type in ['both', 'fragment']:
                    print(80*'-')
                    print_fragment_header(bytesbuffer)
                bytesbuffer = bytearray()
                nfrag += 1
            bytesbuffer.extend(byte)
    return


def parse_args():
    parser = argparse.ArgumentParser(
        description='Python script to parse binary files converted from HDF5.')

    parser.add_argument('-f', '--file_name',
                        help='path to binary file',
                        required=True)

    parser.add_argument('-p', '--print-headers',
                        choices=['trigger', 'fragment', 'both'],
                        default='both',
                        help='select which header data to display')

    parser.add_argument('-l', '--list-components',
                        help='''list components in trigger record header, used
                        together with "--print-headers trigger" or
                        "--print-headers both"''', action='store_true')

    parser.add_argument('-d', '--debug',
                        help='print locations of headers in the file',
                        action='store_true')

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

    bfile = args.file_name
    print("Reading file", bfile)

    read_binary_file(bfile)
    if args.debug:
        get_record_locations_from_binary_file(bfile)
    return


if __name__ == "__main__":
    main()
