#!/bin/sh
# Test of the string <-> model code of the instrument editor (tools/insted.bas) in the console host, without a window: INSTED_HEADLESS=1 makes the tool
# run its self-test (every factory instrument is parsed, rebuilt and parsed again; odd input; the strings are checked by INSTRUMENT) and end.
# The factory strings are compared with synth.c by synthtest1. Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
out=$(INSTED_HEADLESS=1 timeout 60 ./engine1999c tools/insted.bas 2>&1)
if [ "$out" = "insted selftest ok 16" ]; then
    echo "insted     ok"
    exit 0
fi
echo "FAILED: instedtest (output: $out)"
exit 1
