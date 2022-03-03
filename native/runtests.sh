#!/bin/bash
#
# SYNOPSIS
#  ./runtests.sh [test_pattern]
#
# SUMMARY
#
# Runs all tests, captures output to log report PASS, FAIL and overall status
# A successful process must exit with reporting
#    report(0x8000000d)
#    exit(0x00000000)
# Each test must run within the set TEST_TIMEOUT or it will timeout and be
# considered a failure
#
# OPTIONS
#
# No arguments are required, by default the script will run fusesoc sim
# with the CAPPUCCINO pipeline.
#
# Arg 1     [test_pattern] A glob of the tests to run.  The default is or1k-*.
#
# ENVIRONMENT VARIABLES
#
# TARGET      argument to specify which fusesoc target (test bench) to run,
#             i.e. mor1kx_tb, marocchino_tb
# TARGET_ARGS arguments to send to fusesoc target directly, i.e. --tool=verilator
# CORE_ARGS   arguments to send to mor1kx-generic, i.e. --pipeline CAPPUCCINO
# EXPECTED_FAILURES whitespace separated list of test cases that are expected
# to fail.
#
# RETURN VALUE
# Returns 0 is there are no unexpected_fails and no unpexpected_passes.  If
# there are valid unexpected passes one should adjust the EXPECTED_FAILURES
# ENVIRONMENT VARIABLE.

DIR=`dirname $0`
CORE=mor1kx-generic
TEST_PATTERN=$1 ; shift
TEST_TIMEOUT=${TEST_TIMEOUT:-3m}

if [ -z $TEST_PATTERN ] ; then
  TEST_PATTERN="or1k-*"
fi

test_count=0
expected_fail_count=0
timeout_count=0
unexpected_fail_count=0
unexpected_pass_count=0

PASS="\e[32mPASS\e[0m"
FAIL="\e[31mFAIL\e[0m"

TAP_REPORT_FILE=report.tap

function initialize_tap_report {
  touch $TAP_REPORT_FILE
}

function pass {
  echo "ok $@" >> $TAP_REPORT_FILE
}

function fail {
  echo "not ok $@" >> $TAP_REPORT_FILE
}

function end_tap_report {
  echo "1..$1" >> $TAP_REPORT_FILE
}

if [ ! -d $DIR/build/or1k ] ; then
  echo "Cannot find any tests, did you build them?"
  exit 1
fi

echo "Running test with test filter: '$TEST_PATTERN' timeout: '$TEST_TIMEOUT'"
if [ "$TARGET_ARGS" ] ; then
  echo "  TARGET_ARGS '$TARGET_ARGS'"
fi
if [ "$CORE_ARGS" ] ; then
  echo "  CORE_ARGS '$CORE_ARGS'"
fi
if [ "$EXPECTED_FAILURES" ] ; then
  echo "  EXPECTED_FAILURES '$EXPECTED_FAILURES'"
fi
if [ "$ARTIFACT_PATH" ] ; then
  echo "  ARTIFACT_PATH '$ARTIFACT_PATH'"
fi

if [ -z "$TARGET" ] ; then
  TARGET=mor1kx_tb
fi
echo
echo > runtests.log

# run tests
sigint_exit=
inthandler() {
  sigint_exit=y
  if [ "$timeout_pid" ] ; then
    kill -INT -$timeout_pid
  fi
}
trap inthandler SIGINT

initialize_tap_report

for test_path in $DIR/build/or1k/${TEST_PATTERN}; do
  test_name=`basename $test_path`
  test_path=`readlink -f $test_path`
  # pattern to check EXPECTED_FAILURES with word boundary regex
  expected_failure_pattern=\\b$test_name\\b
  ((test_count++))

  test_log=`mktemp -t $test_name.XXX.log`

  date -u -Iseconds > $test_log
  echo "Running: fusesoc run --target $TARGET $TARGET_ARGS $CORE --elf_load $test_path $CORE_ARGS" >> $test_log

  printf "%-60s" "Running $test_name"
  timeout $TEST_TIMEOUT fusesoc run --target $TARGET $TARGET_ARGS $CORE --elf_load $test_path $CORE_ARGS >> $test_log 2>&1 &
  timeout_pid=$!

  if ! wait $timeout_pid ; then
    echo "TIME OUT"
    fail $test_name "TIME OUT"
    ((timeout_count++))
  else
    if grep -q "exit(0x00000000)" $test_log ; then
      if [ "$EXPECTED_FAILURES" ] && [[ "$EXPECTED_FAILURES" =~ $expected_failure_pattern ]] ; then
        echo "UNEXPECTED PASS"
        fail $test_name "UNEXPECTED PASS"
        ((unexpected_pass_count++))
      else
        echo -e "$PASS"
        pass $test_name
      fi
    else
      if [ "$EXPECTED_FAILURES" ] && [[ "$EXPECTED_FAILURES" =~ $expected_failure_pattern ]] ; then
        echo -e "$FAIL"
        pass $test_name
        ((expected_fail_count++))
      else
        echo "UNEXPECTED FAIL"
        fail $test_name "UNEXPECTED FAIL"
        ((unexpected_fail_count++))
      fi
    fi
  fi
  timeout_pid=

  cat $test_log >> runtests.log
  rm $test_log

  if [ "$ARTIFACT_PATH" ] ; then
    mkdir -p artifacts/$test_name
    cp $ARTIFACT_PATH/*.{log,vcd} artifacts/$test_name/
  fi

  if [ "$sigint_exit" ] ; then
    exit 1
  fi
done

# finish up

end_tap_report $test_count

printf "%-60sTotal: %3d\n"               "Results" $test_count
printf "%-60sExpected Failures:   %3d\n" " "       $expected_fail_count
printf "%-60sUnexpected Failures: %3d\n" " "       $unexpected_fail_count
printf "%-60sUnexpected Pass:     %3d\n" " "       $unexpected_pass_count
printf "%-60sTimeouts:            %3d\n" " "       $timeout_count

if [ $unexpected_fail_count -gt 0 ] \
   || [ $unexpected_pass_count -gt 0 ] \
   || [ $timeout_count -gt 0 ] ; then
  echo "FAILURE"
  exit 1
fi

echo "SUCCESS"
exit 0
