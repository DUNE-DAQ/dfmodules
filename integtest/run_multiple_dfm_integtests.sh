# 30-Aug-2023, KAB

# handle command-line arguments
if [ $# -gt 0 ]; then
    if [ "$1" == "-?" ] || [ "$1" == "-h" ] || [ "$1" == "--help" ]; then
        echo "Usage: $0 <session number> <number of tests>"
        exit
    fi
fi
let session_number=1
if [ $# -gt 0 ]; then
    let session_number=$1
fi
let number_of_tests=99
if [ $# -gt 1 ]; then
    let number_of_tests=$2
fi

# other setup
TIMESTAMP=`date '+%Y%m%d%H%M%S'`
mkdir -p /tmp/pytest-of-${USER}
ITGRUNNER_LOG_FILE="/tmp/pytest-of-${USER}/dfmodules_integtest_bundle_${TIMESTAMP}.log"

# run the tests
if [ $number_of_tests -ge 1 ]; then
  TEST_NAME="large_trigger_record_test.py"
  echo "===== Running ${TEST_NAME}" >> ${ITGRUNNER_LOG_FILE}
  pytest -s ${TEST_NAME} --nanorc-option partition-number ${session_number} | tee -a ${ITGRUNNER_LOG_FILE}
fi
if [ $number_of_tests -ge 2 ]; then
  TEST_NAME="disabled_output_test.py"
  echo "===== Running ${TEST_NAME}" >> ${ITGRUNNER_LOG_FILE}
  pytest -s ${TEST_NAME} --nanorc-option partition-number ${session_number} | tee -a ${ITGRUNNER_LOG_FILE}
fi
if [ $number_of_tests -ge 3 ]; then
  TEST_NAME="multi_output_file_test.py"
  echo "===== Running ${TEST_NAME}" >> ${ITGRUNNER_LOG_FILE}
  pytest -s ${TEST_NAME} --nanorc-option partition-number ${session_number} | tee -a ${ITGRUNNER_LOG_FILE}
fi
if [ $number_of_tests -ge 4 ]; then
  TEST_NAME="insufficient_disk_space_test.py"
  echo "===== Running ${TEST_NAME}" >> ${ITGRUNNER_LOG_FILE}
  pytest -s ${TEST_NAME} --nanorc-option partition-number ${session_number} | tee -a ${ITGRUNNER_LOG_FILE}
fi

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
