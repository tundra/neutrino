#!/bin/sh
# Copyright 2013 the Neutrino authors (see AUTHORS).
# Licensed under the Apache License, Version 2.0 (see LICENSE).

# Runner script for the golden tests. A golden test is the simplest kind (which
# also means that you only want to write them if there's no other way). Runs a
# program through the runtime and compares the output on stdout with the
# expected ("golden") output given along with the test case.

# Parse command-line arguments.

TEST_FILE=
OUTPUT_FILE=
EXECUTABLE=

while getopts "t:o:e:" OPT; do
  case "$OPT" in
    t)
      TEST_FILE="$OPTARG"
      ;;
    o)
      OUTPUT_FILE="$OPTARG"
      ;;
    e)
      EXECUTABLE="$OPTARG"
      ;;
  esac
done

# Clear the output file so we're ready to create a new one.

rm -f "$OUTPUT_FILE"

# Scan through the test file and pick out INPUT/EXPECT clauses. This uses a
# state machine-like structure that stores them in variables and when the
# variables have both been set executes the test and starts over.

INPUT=
EXPECT=
I=0
while read LINE; do
  # Strip end-of-line comments.
  LINE=$(echo "$LINE" | sed -e s/^\\s*#.*$//g)
  if echo "$LINE" | grep INPUT: > /dev/null; then
    # Found an input clause.
    INPUT=$(echo "$LINE" | sed -e s/^INPUT:\\s*//g)
  elif echo "$LINE" | grep EXPECT: > /dev/null; then
    # Found an expect clause.
    EXPECT=$(echo "$LINE" | sed -e s/^EXPECT:\\s*//g)
  elif [ -n "$INPUT" -a ! -n "$EXPECT" ]; then
    # There's already some input but no expect clause append to the input.
    INPUT="$INPUT $LINE"
  elif [ -n "$LINE" ]; then
    # Any nonempty line that didn't match above is an error; report it.
    printf "Unexpected line '%s'\\n" "$LINE"
    exit 1
  fi
  if [ -n "$INPUT" -a -n "$EXPECT" ]; then
    # Print a progress marker from 0-9 with a space between blocks of 10.
    echo -n $I
    I=$((I + 1))
    if [ "$I" -eq "10" ]; then
      I=0
      echo -n " "
    fi
    # If we now have both an INPUT and an EXPECT line run the test.
    FOUND=$(./src/python/neutrino/main.py --expression "$INPUT" | $EXECUTABLE - 2>&1)
    if [ "$EXPECT" != "$FOUND" ]; then
      # Test failed. Display an error.
      echo
      printf "Error on input '%s':\\n" "$INPUT"
      printf "  Expected: '%s'\\n" "$EXPECT"
      printf "  Found: '%s'\\n" "$FOUND"
      exit 1
    fi
    # Clear the variables so we're ready for the next round.
    INPUT=
    EXPECT=
  fi
done < "$TEST_FILE"

# We completed successfully so we can signal success by touching the output
# file (as well as implicitly exiting 0).

echo
touch "$OUTPUT_FILE"
