#!/bin/sh
# Test of the house generator tools/housegen.bas (the design study of the ruins of the static world): HOUSEGEN_TEST=1 makes it check the invariants of 300 generated
# houses (sizes, pitch, rooms inside the footprint and not overlapping, doors, windows on their walls, furniture inside the walls) in the console host, without graphics.
# Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
out=$(HOUSEGEN_TEST=1 timeout 120 ./engine1999c tools/housegen.bas < /dev/null 2>&1)
status=$?
echo "$out" | tail -n 5
if [ "$status" -ne 0 ]; then echo "FAILED: housegen selftest (status $status)"; exit 1; fi
echo "$out" | grep -q "300 houses, 0 failures" || { echo "FAILED: housegen selftest (no result line)"; exit 1; }
exit 0
