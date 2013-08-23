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
while read LINE; do
  # Strip end-of-line comments.
  LINE=$(echo "$LINE" | sed -e s/^\\s*#.*$//g)
  # Check if this is an input line.
  if echo "$LINE" | grep INPUT: > /dev/null; then
    INPUT=$(echo "$LINE" | sed -e s/^INPUT:\\s*//g)
  fi
  # Check if this is an expect line.
  if echo "$LINE" | grep EXPECT: > /dev/null; then
    EXPECT=$(echo "$LINE" | sed -e s/^EXPECT:\\s*//g)
  fi
  if [ -n "$INPUT" -a -n "$EXPECT" ]; then
    # If we now have both an INPUT and an EXPECT line run the test.
    FOUND=$(./src/python/neutrino/main.py --expression "$INPUT" | $EXECUTABLE - 2>&1)
    if [ "$EXPECT" != "$FOUND" ]; then
      # Test failed. Display an error.
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

touch "$OUTPUT_FILE"
