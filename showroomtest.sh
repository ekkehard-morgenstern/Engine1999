#!/bin/sh
# Test of the demo demos/showroom.bas in the console host without graphics (SHOWROOM_HEADLESS=1): the map is read through the library w3dmap and the program walks
# through it: sectors over each other, a sloped floor, a lift (W3D SECMOVE), glass that blocks, a round room. Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
out=$(SHOWROOM_HEADLESS=1 timeout 60 ./engine1999c demos/showroom.bas 2>&1)
fails=$(printf '%s\n' "$out" | grep -c '^FAIL\|^?')
oks=$(printf '%s\n' "$out" | grep -c '^ok ')
if [ "$fails" = 0 ] && [ "$oks" = 14 ]; then
    echo "showroom   ok"
    exit 0
fi
echo "FAILED: showroomtest ($oks ok, $fails failed)"
printf '%s\n' "$out" | grep -v '^ok ' | head -n 10
exit 1
