#!/usr/bin/python3

# Version 0.1
# Last modified: January 23, 2021

# USAGE: python3 hdf5_dump.py -f sample.hdf5 -TRH

import os
import time
import argparse
import subprocess
import re

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

def bytes_to_int(bytes):
  return int(binascii.hexlify(bytes), 16)

## Print FragmentHeader
def print_frag_header(data_array):
  check_word = data_array[0:4]
  version = data_array[4:8]
  fragment_size = data_array[8:16]
  trigger_number = data_array[16:24]
  trigger_timestamp = data_array[24:32]
  window_begin = data_array[32:40]
  window_end = data_array[40:48]
  run_number = data_array[48:52]
  geo_id_apa = data_array[52:56]
  geo_id_link = data_array[56:60]
  error_bits = data_array[60:64]
  fragment_type = data_array[64:68]

  print("Magic word:\t\t", binascii.hexlify(check_word)[::-1])
  print("Version:\t\t", bytes_to_int((version)[::-1]))
  print("Frag size:\t\t", bytes_to_int((fragment_size)[::-1]))
  print("Trig number:\t\t", bytes_to_int((trigger_number)[::-1]))
  print("Trig timestamp:\t\t", bytes_to_int((trigger_timestamp)[::-1]),
        "("+str(datetime.datetime.fromtimestamp(float(bytes_to_int((trigger_timestamp)[::-1]))/CLOCK_SPEED_HZ))+")")
  print("Window begin:\t\t", bytes_to_int((window_begin)[::-1]),
        "("+str(datetime.datetime.fromtimestamp(float(bytes_to_int((window_begin)[::-1]))/CLOCK_SPEED_HZ))+")")
  print("Window end:\t\t", bytes_to_int((window_end)[::-1]),
        "("+str(datetime.datetime.fromtimestamp(float(bytes_to_int((window_end)[::-1]))/CLOCK_SPEED_HZ))+")")
  print("Run number:\t\t", bytes_to_int((run_number)[::-1]))
  print("GeoID (APA):\t\t", bytes_to_int((geo_id_apa)[::-1]))
  print("GeoID (link):\t\t", bytes_to_int((geo_id_link)[::-1]))
  print("Error bits:\t\t", bytes_to_int((error_bits)[::-1]))
  print("Fragment type:\t\t", bytes_to_int((fragment_type)[::-1]))


## Print TriggerRecordHeader
def print_trh(data_array):
  check_word = data_array[0:4]
  version = data_array[4:8]
  trigger_number = data_array[8:16]
  trigger_timestamp= data_array[16:24]
  numb_req_comp = data_array[24:32]
  run_number = data_array[32:36]
  error_bits = data_array[36:40]
  trigger_type = data_array[40:42]
  unused = data_array[42:48]

  print("Magic word:\t\t", binascii.hexlify(check_word)[::-1])
  #print("Trigger number: ", binascii.hexlify(trigger_number)[::-1])
  print("Version:\t\t", bytes_to_int((version)[::-1]))
  print("Trig number:\t\t", bytes_to_int((trigger_number)[::-1]))
  print("Trig timestamp:\t\t", bytes_to_int((trigger_timestamp)[::-1]),
        "("+str(datetime.datetime.fromtimestamp(float(bytes_to_int((trigger_timestamp)[::-1]))/CLOCK_SPEED_HZ))+")")
  print("Num req comp:\t\t", bytes_to_int((numb_req_comp)[::-1]))
  print("Run number:\t\t", bytes_to_int((run_number)[::-1]))
  print("Error bits:\t\t", bytes_to_int((error_bits)[::-1]))
  print("Trigger type:\t\t", bytes_to_int((trigger_type)[::-1]))

  for i in range(bytes_to_int(numb_req_comp[::-1])):
      i_geo_id_apa = data_array[48+i*24: 52+i*24]
      i_geo_id_link = data_array[52+i*24: 56+i*24]
      print("Expected Component: \t\t", i)
      print("       GeoID (APA): \t\t", bytes_to_int((i_geo_id_apa)[::-1]))
      print("       GeoID (link): \t\t", bytes_to_int((i_geo_id_link)[::-1]))



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
            print("nheader:", nheader)
            print('Path:', name)
            print('Size:', dset.shape)
            print('Data type:', dset.dtype)
            nheader += 1
            data_array = bytearray(dset[:])
            print_trh(data_array)
        if not printTRH and "TriggerRecordHeader" not in name:
            print("nheader:", nheader)
            print('Path:', name)
            print('Size:', dset.shape)
            print('Data type:', dset.dtype)
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
