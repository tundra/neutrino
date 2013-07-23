#!/bin/sh

# Filters output lines that look like stack traces through addr2line to
# resolve line files and line numbers, if possible.

while read L; do
  # The stack-like lines we're looking for.
  EXPR="^.*\s\(\S*\)(\(.*\)) \[\(0x[0-9a-fA-F]*\)\].*$"
  FOUND=$(echo "$L" | grep "$EXPR")
  if [ "$FOUND" = "" ]; then
    # The line didn't look like a stack entry.
    echo "$L"
  else
    # The line did look like a stack entry; extract the arguments to addr2line.
    ARGS=$(echo "$L" | sed -e "s/$EXPR/-e \1 \3/g")
    LINE=$(addr2line $ARGS)
    # Print the full line, annotated with the addr2line output.
    echo "$L {$LINE}"
  fi
done
