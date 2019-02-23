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
# SIM_ARGS  arguments to send to fusesoc sim directly, i.e. --sim=verilator
# CORE_ARGS arguments to send to mor1kx-generic, i.e. --pipeline CAPPUCCINO

DIR=`dirname $0`
TARGET=mor1kx_cappuccino
CORE=mor1kx-generic
PIPELINE=$1 ; shift
TEST_PATTERN=$1 ; shift
TEST_TIMEOUT="3m"

if [ -z $PIPELINE ] ; then
  PIPELINE=CAPPUCCINO
fi

if [ -z $TEST_PATTERN ] ; then
  TEST_PATTERN="or1k-*"
fi

test_count=0
fail_count=0

PASS="\e[32mPASS\e[0m"
FAIL="\e[31mFAIL\e[0m"

if [ ! -d $DIR/build/or1k ] ; then
  echo "Cannot find any tests, did you build them?"
  exit 1
fi

echo "Running test with test filter: $TEST_PATTERN"
if [ "$SIM_ARGS" ] ; then
  echo "  SIM_ARGS  '$SIM_ARGS'"
fi
if [ "$CORE_ARGS" ] ; then
  echo "  CORE_ARGS '$CORE_ARGS'"
fi
echo
echo > runtests.log

# run tests
sigint_exit=
inthandler() {
  sigint_exit=y
  if [ ! -z "$timeout_pid" ] ; then
    kill -INT -$timeout_pid
  fi
}
trap inthandler SIGINT

for test_path in $DIR/build/or1k/${TEST_PATTERN}; do
  test_name=`basename $test_path`
  ((test_count++))

  test_log=`mktemp -t $test_name.XXX.log`

  date -u -Iseconds > $test_log
  echo "Running: fusesoc sim $SIM_ARGS $CORE --elf-load $test_path $CORE_ARGS" >> $test_log

  printf "%-60s" "Running $test_name"
  timeout $TEST_TIMEOUT fusesoc sim $SIM_ARGS $CORE --elf-load $test_path $CORE_ARGS >> $test_log 2>&1 &
  timeout_pid=$!

  if ! wait $timeout_pid ; then
    echo "TIME OUT"
    ((fail_count++))
  else
    if grep -q "exit(0x00000000)" $test_log ; then
      echo -e "$PASS"
    else
      echo -e "$FAIL"
      ((fail_count++))
    fi
  fi
  timeout_pid=

  cat $test_log >> runtests.log
  rm $test_log

  if [ "$sigint_exit" ] ; then
    exit 1
  fi
done

# finish up

printf "%-60sTotal: %3d  FAIL: %3d\n" "Results" $test_count $fail_count

if [ $fail_count -gt 0 ] ; then
  exit 1
fi
exit 0
