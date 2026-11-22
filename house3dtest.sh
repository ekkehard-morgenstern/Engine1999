#!/bin/sh
# Test of the 3D house tools/house3d.bas (the ruins of the static world): H3_TEST=1 makes it check the generator part (ridge height, rooms, the garage annex of type 2 with
# its doors, windows on their walls, furniture inside the rooms) of 100 houses of each type in the console host, without graphics. Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
out=$(H3_TEST=1 timeout 120 ./engine1999c tools/house3d.bas < /dev/null 2>&1)
status=$?
echo "$out" | tail -n 5
if [ "$status" -ne 0 ]; then echo "FAILED: house3d selftest (status $status)"; exit 1; fi
echo "$out" | grep -q "200 houses, 0 failures" || { echo "FAILED: house3d selftest (no result line)"; exit 1; }
exit 0
