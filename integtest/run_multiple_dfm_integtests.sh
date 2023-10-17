#!/bin/bash
# 11-Oct-2023, KAB

integtest_list=( "large_trigger_record_test.py" "disabled_output_test.py" "multi_output_file_test.py" "insufficient_disk_space_test.py" )

usage() {
    declare -r script_name=$(basename "$0")
    echo """
Usage:
"${script_name}" [option(s)]

Options:
    -h, --help
    -s <DAQ session number (formerly known as partition number), default=1)>
    -f <zero-based index of the first test to be run, default=0>
    -l <zero-based index of the last test to be run, default=999>
    -n <number of times to run each individual test, default=1>
    -N <number of times to run the full set of selected tests, default=1>
"""
    let counter=0
    echo "List of available tests:"
    for tst in ${integtest_list[@]}; do
        echo "    ${counter}: $tst"
        let counter=${counter}+1
    done
    echo ""
}

TEMP=`getopt -o hs:f:l:n:N: --long help -- "$@"`
eval set -- "$TEMP"

let session_number=1
let first_test_index=0
let last_test_index=999
let individual_run_count=1
let overall_run_count=1

while true; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -s)
            let session_number=$2
            shift 2
            ;;
        -f)
            let first_test_index=$2
            shift 2
            ;;
        -l)
            let last_test_index=$2
            shift 2
            ;;
        -n)
            let individual_run_count=$2
            shift 2
            ;;
        -N)
            let overall_run_count=$2
            shift 2
            ;;
        --)
            shift
            break
            ;;
    esac
done

# other setup
TIMESTAMP=`date '+%Y%m%d%H%M%S'`
mkdir -p /tmp/pytest-of-${USER}
ITGRUNNER_LOG_FILE="/tmp/pytest-of-${USER}/dfmodules_integtest_bundle_${TIMESTAMP}.log"

# run the tests
let overall_loop_count=0
while [[ ${overall_loop_count} -lt ${overall_run_count} ]]; do

  let test_index=0
  for TEST_NAME in ${integtest_list[@]}; do
    if [[ ${test_index} -ge ${first_test_index} && ${test_index} -le ${last_test_index} ]]; then

      let individual_loop_count=0
      while [[ ${individual_loop_count} -lt ${individual_run_count} ]]; do
        echo "===== Running ${TEST_NAME}" >> ${ITGRUNNER_LOG_FILE}
        pytest -s ${TEST_NAME} --nanorc-option partition-number ${session_number} | tee -a ${ITGRUNNER_LOG_FILE}

        let individual_loop_count=${individual_loop_count}+1
      done

    fi
    let test_index=${test_index}+1
  done

  let overall_loop_count=${overall_loop_count}+1
done

# print out summary information
echo ""
echo ""
echo "+++++++++++++++++++++++++++++++++++++++++++++++++"
echo "++++++++++++++++++++ SUMMARY ++++++++++++++++++++"
echo "+++++++++++++++++++++++++++++++++++++++++++++++++"
echo ""
date
echo "Log file is: ${ITGRUNNER_LOG_FILE}"
echo ""
grep '=====' ${ITGRUNNER_LOG_FILE} | egrep ' in |Running'
