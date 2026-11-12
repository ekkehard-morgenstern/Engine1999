#!/bin/sh
# Test of the model code of the map editor (tools/mapedit.bas) in the console host, without a window (MAPEDIT_HEADLESS=1): sectors as polygons, point in
# polygon, welding of portals (sectors whose heights do not overlap stay apart), building the world, saving and loading a map, importing it back.
# Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
out=$(MAPEDIT_HEADLESS=1 timeout 60 ./engine1999c tools/mapedit.bas 2>&1)
fails=$(printf '%s\n' "$out" | grep -c '^FAIL\|^?')
oks=$(printf '%s\n' "$out" | grep -c '^ok ')
if [ "$fails" = 0 ] && [ "$oks" = 13 ]; then
    echo "mapedit    ok"
    exit 0
fi
echo "FAILED: mapedittest ($oks ok, $fails failed)"
printf '%s\n' "$out" | grep -v '^ok ' | head -n 10
exit 1
