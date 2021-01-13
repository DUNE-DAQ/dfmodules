#!/usr/bin/python3

# Version 0.1
# Last modified: January 13, 2021

# USAGE: python3 hdf5_dump.py -f sample.hdf5 -TRH  

import os
import time
import argparse
import subprocess
import re

import h5py
import binascii

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

    parser.add_argument('-v', '--version', action='version')
    return parser.parse_args()


# Tools
         
## Recursive function to get all the information         
## for a given HDF5 file_name
def traverse_datasets(hdf_file):
  def h5py_dataset_iterator(hdf5_group, prefix=''):
    for key in hdf5_group.keys():
      item = hdf5_group[key]
      path = f'{prefix}/{key}'
      if isinstance(item, h5py.Dataset): # test for dataset
        yield (path, item)
      elif isinstance(item, h5py.Group): # test for group (go down)
       yield from h5py_dataset_iterator(item, path) 

  for path, _ in h5py_dataset_iterator(hdf_file):
    yield path


## Recursive function to get only 
## the trigger record headers
def traverse_TRH_datasets(hdf_file):
  def h5py_dataset_iterator(hdf5_group, prefix=''):
    for key in hdf5_group.keys():
      item = hdf5_group[key]
      path = f'{prefix}/{key}'
      if isinstance(item, h5py.Dataset) and key == "TriggerRecordHeader": # test for dataset
        yield (path, item)
      elif isinstance(item, h5py.Group): # test for group (go down)
       yield from h5py_dataset_iterator(item, path) 

  for path, _ in h5py_dataset_iterator(hdf_file):
    yield path


def bytes_to_int(bytes):
  return int(binascii.hexlify(bytes), 16)

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

  print("Magic word:\t\t", binascii.hexlify(check_word)[::-1])
  #print("Trigger number: ", binascii.hexlify(trigger_number)[::-1])
  print("Version:\t\t", bytes_to_int((version)[::-1]))
  print("Trig number:\t\t", bytes_to_int((trigger_number)[::-1]))
  print("Trig timestamp:\t\t", bytes_to_int((trigger_timestamp)[::-1]))
  print("Num req comp:\t\t", bytes_to_int((numb_req_comp)[::-1]))
  print("Run number:\t\t", bytes_to_int((run_number)[::-1]))
  print("Error bits:\t\t", bytes_to_int((error_bits)[::-1]))
  print("Trigger type:\t\t", bytes_to_int((trigger_type)[::-1]))




# Actions


## Parse the TriggerRecordHeader data 
def get_trigger_record_headers(file_name):
  with h5py.File(file_name, 'r') as f:
    for dset in traverse_TRH_datasets(f):
      print('Path:', dset)
      print('Size:', f[dset].shape)
      print('Data type:', f[dset].dtype)
      #print('Data content:', f[dset][:])
      #print(binascii.hexlify(bytearray(f[dset][:])))
      print_trh(bytearray(f[dset][:]))
      print("============================================")


## Dump the contents of the HDF5 file
## Similar to the h5dump tool
def dump_file(file_name):
  with h5py.File(file_name, 'r') as f:
    for dset in traverse_datasets(f):
      print('Path:', dset)
      print('Size:', f[dset].shape)
      print('Data type:', f[dset].dtype)
      #print(binascii.hexlify(bytearray(f[dset][:])))


def main():
    args=parse_args()

     
    input_path = args.path
    input_file_name = args.file_name
    full_path = input_path + input_file_name  
 
    print("Reading file", full_path ) 
    if args.header_only:
      dump_file(full_path)

    if args.TRH_only:
      get_trigger_record_headers(full_path)

if __name__ == "__main__":
    main()


