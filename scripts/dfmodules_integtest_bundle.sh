#!/bin/bash
# 29-Apr-2025, KAB

integtest_list=( "multiple_data_writers_test.py" "insufficient_disk_space_test.py" "large_trigger_record_test.py" "hdf5_compression_test.py" "disabled_output_test.py" "max_file_size_test.py" "trmonrequestor_test.py" )
let last_test_index=${#integtest_list[@]}-1

usage() {
    declare -r script_name=$(basename "$0")
    echo """
Usage:
"${script_name}" [option(s)]

Options:
    -h, --help : prints out usage information
    -f <zero-based index of the first test to be run, default=0>
    -l <zero-based index of the last test to be run, default=${last_test_index}>
    -k <pipe-delimited string to select which tests will be run ('egrep -i' match to test name)>
    -n <number of times to run each individual test, default=1>
    -N <number of times to run the full set of selected tests, default=1>
    --stop-on-failure : causes the script to stop when one of the integtests reports a failure
    --stop-on-skip : causes the script to stop when one of the integtests skips a test
"""
    let counter=0
    echo "List of available tests:"
    for tst in ${integtest_list[@]}; do
        echo "    ${counter}: $tst"
        let counter=${counter}+1
    done
    echo ""
}

TEMP=`getopt -o hs:f:l:k:n:N: --long help,stop-on-failure,stop-on-skip -- "$@"`
eval set -- "$TEMP"

let first_test_index=0
let individual_test_requested_iterations=1
let full_set_requested_interations=1
let stop_on_failure=0
let stop_on_skip=0
requested_test_names=

while true; do
    case "$1" in
        -h|--help)
            usage
            exit 0
            ;;
        -f)
            let first_test_index=$2
            shift 2
            ;;
        -l)
            let last_test_index=$2
            shift 2
            ;;
        -k)
            requested_test_names=$2
            shift 2
            ;;
        -n)
            let individual_test_requested_iterations=$2
            shift 2
            ;;
        -N)
            let full_set_requested_interations=$2
            shift 2
            ;;
        --stop-on-failure)
            let stop_on_failure=1
            shift
            ;;
        --stop-on-skip)
            let stop_on_skip=1
            shift
            ;;
        --)
            shift
            break
            ;;
    esac
done

# check if the numad daemon is running
numad_grep_output=`ps -ef | grep numad | grep -v grep`
if [[ "${numad_grep_output}" != "" ]]; then
    echo "*********************************************************************"
    echo "*** DANGER, DANGER, 'numad' appears to be running on this computer!"
    echo "*** 'ps' output:  ${numad_grep_output}"
    echo "*** <ctrl-c> now if you want to abort this testing."
    echo "*********************************************************************"
    sleep 3
fi

# other setup
TIMESTAMP=`date '+%Y%m%d%H%M%S'`
mkdir -p /tmp/pytest-of-${USER}
ITGRUNNER_LOG_FILE="/tmp/pytest-of-${USER}/dfmodules_integtest_bundle_${TIMESTAMP}.log"

let number_of_individual_tests=0
let test_index=0
for TEST_NAME in ${integtest_list[@]}; do
    if [[ ${test_index} -ge ${first_test_index} && ${test_index} -le ${last_test_index} ]]; then
        requested_test=`echo ${TEST_NAME} | egrep -i ${requested_test_names:-${TEST_NAME}}`
        if [[ "${requested_test}" != "" ]]; then
            let number_of_individual_tests=${number_of_individual_tests}+1
        fi
    fi
    let test_index=${test_index}+1
done
let total_number_of_tests=${number_of_individual_tests}*${individual_test_requested_iterations}*${full_set_requested_interations}

# run the tests
let overall_test_index=0  # this is only used for user feedback
let full_set_loop_count=0
while [[ ${full_set_loop_count} -lt ${full_set_requested_interations} ]]; do
    let test_index=0
    for TEST_NAME in ${integtest_list[@]}; do
        if [[ ${test_index} -ge ${first_test_index} && ${test_index} -le ${last_test_index} ]]; then
            requested_test=`echo ${TEST_NAME} | egrep -i ${requested_test_names:-${TEST_NAME}}`
            if [[ "${requested_test}" != "" ]]; then
                let individual_loop_count=0
                while [[ ${individual_loop_count} -lt ${individual_test_requested_iterations} ]]; do
                    let overall_test_index=${overall_test_index}+1
                    echo ""
                    echo -e "\U0001F535 \033[0;34mStarting test ${overall_test_index} of ${total_number_of_tests}...\033[0m \U0001F535" | tee -a ${ITGRUNNER_LOG_FILE}

                    echo -e "\u2B95 \033[0;1mRunning ${TEST_NAME}\033[0m \u2B05" | tee -a ${ITGRUNNER_LOG_FILE}
                    if [[ -e "./${TEST_NAME}" ]]; then
                        pytest -s ./${TEST_NAME} | tee -a ${ITGRUNNER_LOG_FILE}
                    elif [[ -e "${DBT_AREA_ROOT}/sourcecode/dfmodules/integtest/${TEST_NAME}" ]]; then
                        if [[ -w "${DBT_AREA_ROOT}" ]]; then
                            pytest -s ${DBT_AREA_ROOT}/sourcecode/dfmodules/integtest/${TEST_NAME} | tee -a ${ITGRUNNER_LOG_FILE}
                        else
                            pytest -s -p no:cacheprovider ${DBT_AREA_ROOT}/sourcecode/dfmodules/integtest/${TEST_NAME} | tee -a ${ITGRUNNER_LOG_FILE}
                        fi
                    else
                        pytest -s -p no:cacheprovider ${DFMODULES_SHARE}/integtest/${TEST_NAME} | tee -a ${ITGRUNNER_LOG_FILE}
                    fi
                    let pytest_return_code=${PIPESTATUS[0]}

                    let individual_loop_count=${individual_loop_count}+1

                    if [[ ${stop_on_failure} -gt 0 ]]; then
                        if [[ ${pytest_return_code} -ne 0 ]]; then
                            break 3
                        fi
                    fi
                    if [[ ${stop_on_skip} -gt 0 ]]; then
                        search_result=`tail -20 ${ITGRUNNER_LOG_FILE} | grep -i skip`
                        #echo "skip search result is ${search_result}"
                        if [[ ${search_result} != "" ]]; then
                            break 3
                        fi
                    fi
                done
            fi
        fi
        let test_index=${test_index}+1
    done

    let full_set_loop_count=${full_set_loop_count}+1
done

# print out summary information
echo ""                                                   | tee -a ${ITGRUNNER_LOG_FILE}
echo ""                                                   | tee -a ${ITGRUNNER_LOG_FILE}
echo "+++++++++++++++++++++++++++++++++++++++++++++++++"  | tee -a ${ITGRUNNER_LOG_FILE}
echo "++++++++++++++++++++ SUMMARY ++++++++++++++++++++"  | tee -a ${ITGRUNNER_LOG_FILE}
echo "+++++++++++++++++++++++++++++++++++++++++++++++++"  | tee -a ${ITGRUNNER_LOG_FILE}
echo ""                                                   | tee -a ${ITGRUNNER_LOG_FILE}
date                                                      | tee -a ${ITGRUNNER_LOG_FILE}
echo "Log file is: ${ITGRUNNER_LOG_FILE}"                 | tee -a ${ITGRUNNER_LOG_FILE}
echo ""                                                   | tee -a ${ITGRUNNER_LOG_FILE}
egrep $'=====|\u2B95' ${ITGRUNNER_LOG_FILE} | egrep ' in |Running' | tee -a ${ITGRUNNER_LOG_FILE}

# check again if the numad daemon is running
numad_grep_output=`ps -ef | grep numad | grep -v grep`
if [[ "${numad_grep_output}" != "" ]]; then
    echo ""                                                                                 | tee -a ${ITGRUNNER_LOG_FILE}
    echo "********************************************************************************" | tee -a ${ITGRUNNER_LOG_FILE}
    echo "*** WARNING: 'numad' appears to be running on this computer!"                     | tee -a ${ITGRUNNER_LOG_FILE}
    echo "*** 'ps' output:  ${numad_grep_output}"                                           | tee -a ${ITGRUNNER_LOG_FILE}
    echo "*** This daemon can adversely affect the running of these tests, especially ones" | tee -a ${ITGRUNNER_LOG_FILE}
    echo "*** that are resource intensive in the Readout Apps. This is because numad moves" | tee -a ${ITGRUNNER_LOG_FILE}
    echo "*** processes (threads?) to different cores/numa nodes periodically, and that"    | tee -a ${ITGRUNNER_LOG_FILE}
    echo "*** context switch can disrupt the stable running of the DAQ processes."          | tee -a ${ITGRUNNER_LOG_FILE}
    echo "********************************************************************************" | tee -a ${ITGRUNNER_LOG_FILE}
fi
