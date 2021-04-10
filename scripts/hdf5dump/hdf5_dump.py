#!/usr/bin/python3

# Version 0.1
# Last modified: January 23, 2021

# USAGE: python3 hdf5_dump.py -f sample.hdf5 -TRH

import os
import time
import argparse
import subprocess
import re
import struct

import h5py
import binascii
import datetime

CLOCK_SPEED_HZ = 50000000.0

def parse_args():
    parser = argparse.ArgumentParser(description='Python script to parse DUNE-DAQ HDF5 output files.')
    parser.version = '0.1'
    parser.add_argument('-p', '--path',
                       help='Path to HDF5 file',
                       default='./')

    parser.add_argument('-f', '--file_name',
                       help='Name of HDF5 file',
                       required=True)

    parser.add_argument('-H', '--header_only',
                       help='Print the header only; no data is displayed',
                       action='store_true')

    parser.add_argument('-TRH', '--TRH_only',
                       help='Print the TriggerRecordheader only; data is displayed',
                       action='store_true')

    parser.add_argument('-num', '--num_req_trig_records',
                        type=int,
                       help='Select number of trigger records to be parsed',
                       default=None)

    parser.add_argument('-v', '--version', action='version')
    return parser.parse_args()

## Print FragmentHeader
def print_frag_header(data_array):
  (check_word, version, fragment_size,
   trigger_number, trigger_timestamp,
   window_begin, window_end,
   runumber, geo_id_apa, geo_id_link,
   error_bits, fragment_type
   ) = struct.unpack('<2I5Q5I', data_array[:68])

  print("Magic word:\t\t", hex(check_word))
  print("Version:\t\t", version)
  print("Frag size:\t\t", fragment_size)
  print("Trig number:\t\t", trigger_number)
  print("Trig timestamp:\t\t {} ({})".format(trigger_timestamp,
        datetime.datetime.fromtimestamp(float(trigger_timestamp)/CLOCK_SPEED_HZ)))
  print("Window begin (ticks@50MHz):\t", window_begin)
  print("Window end (ticks@50MHz):\t", window_end)
  print("Run number:\t\t", runumber)
  print("GeoID (APA):\t\t", geo_id_apa)
  print("GeoID (link):\t\t", geo_id_link)
  print("Error bits:\t\t", error_bits)
  print("Fragment type:\t\t", fragment_type)


## Print TriggerRecordHeader
def print_trh(data_array):
  (check_word, version, trigger_number,
   trigger_timestamp, numb_req_comp, run_number,
   error_bits, trigger_type) = struct.unpack('<2I3Q2IH', data_array[:42])

  print("Magic word:\t\t", hex(check_word))
  print("Version:\t\t", version)
  print("Trig number:\t\t", trigger_number)
  print("Trig timestamp:\t\t", trigger_timestamp,
        "("+str(datetime.datetime.fromtimestamp(float(trigger_timestamp)/CLOCK_SPEED_HZ))+")")
  print("Num req comp:\t\t", numb_req_comp)
  print("Run number:\t\t", run_number)
  print("Error bits:\t\t", error_bits)
  print("Trigger type:\t\t", trigger_type)

  j = 0
  for (i_geo_id_apa, i_geo_id_link, i_begin, i_end) in struct.iter_unpack("<2I2Q", data_array[48:]):
      print("Expected Component: \t\t", j)
      print("       GeoID (APA): \t\t", i_geo_id_apa)
      print("       GeoID (link): \t\t", i_geo_id_link)
      print("       Begin time (ticks@50MHz): \t", i_begin)
      print("       End time (ticks@50MHz): \t\t", i_end)
      j += 1

  return

# Actions

def print_header_func(name, dset):
    global nheader
    #if isinstance(dset, h5py.Group) and "/" not in name:
    #    # This is a new trigger record.
    #    print("New Trigger Record: ", name)
    #if isinstance(dset, h5py.Dataset) and "FELIX" in name:
    #    print("New Component: ", name)
    #if isinstance(dset, h5py.Dataset) and "TriggerRecordHeader" in name:
    #    print("New TriggerRecordHeader : ", name)
    if nheader < NHEADER and isinstance(dset, h5py.Dataset):
        if printTRH and "TriggerRecordHeader" in name:
            print(80*'=')
            print("nheader:\t\t", nheader)
            print('Path:\t\t', name)
            print('Size:\t\t', dset.shape)
            print('Data type:\t\t', dset.dtype)
            nheader += 1
            data_array = bytearray(dset[:])
            print_trh(data_array)
        if not printTRH and "TriggerRecordHeader" not in name:
            print(80*'=')
            print("nheader:\t\t", nheader)
            print('Path:\t\t', name)
            print('Size:\t\t', dset.shape)
            print('Data type:\t\t', dset.dtype)
            nheader += 1
            data_array = bytearray(dset[:])
            print_frag_header(data_array)
    return

nheader = 0
NHEADER = 0
printTRH = False
def get_header(file_name):
    with h5py.File(file_name, 'r') as f:
        f.visititems(print_header_func)
    return


def main():
    args=parse_args()


    input_path = args.path
    input_file_name = args.file_name
    full_path = input_path + input_file_name
    print("Reading file", full_path )

    requested_trigger_records = args.num_req_trig_records
    global NHEADER
    global printTRH
    NHEADER = args.num_req_trig_records

    if args.header_only:
      get_header(full_path)
    if args.TRH_only:
      printTRH = True
      get_header(full_path)

if __name__ == "__main__":
    main()
