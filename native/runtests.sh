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

DIR=`dirname $0`
TARGET=mor1kx_cappuccino
CORE=mor1kx-generic
TEST_TIMEOUT="3m"

test_count=0
fail_count=0

PASS="\e[32mPASS\e[0m"
FAIL="\e[31mFAIL\e[0m"

if [ ! -d $DIR/build/or1k ] ; then
  echo "Cannot find any tests, did you build them?"
  exit 1
fi

TEST_PATTERN=$1
if [ -z $TEST_PATTERN ] ; then
  TEST_PATTERN="or1k-*"
fi

echo > runtests.log

# run tests

for test_path in $DIR/build/or1k/${TEST_PATTERN}; do
  test_name=`basename $test_path`
  ((test_count++))

  test_log=`mktemp -t $test_name.XXX.log`

  date -u -Iseconds > $test_log
  echo "Running: fusesoc sim $CORE --elf-load $test_path" >> $test_log

  printf "%-60s" "Running $test_name"
  if ! timeout $TEST_TIMEOUT fusesoc sim $CORE --elf-load $test_path >> $test_log 2>&1 ; then
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

  cat $test_log >> runtests.log
  rm $test_log
done

# finish up

printf "%-60sTotal: %3d  FAIL: %3d\n" "Results" $test_count $fail_count

if [ $fail_count -gt 0 ] ; then
  exit 1
fi
exit 0
