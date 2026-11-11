#!/bin/sh
# Test of the song / music string code of the tracker (tools/tracker.bas) in the console host, without a window: TRACKER_HEADLESS=1 makes the tool run its
# self-test (note names, piano keys, the PLAY strings of channels with notes, note offs, held notes, rests, instruments, volumes, pan and the start row; editing keys;
# save and open of a song; the exported lines); TRACKER_HEADLESS=<file> also opens a song and lets PLAY check the strings made from it (demos/groove.trk).
# Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
out=$(TRACKER_HEADLESS=1 timeout 60 ./engine1999c tools/tracker.bas 2>&1)
if [ "$out" != "tracker selftest ok 36" ]; then
    echo "FAILED: trackertest (output: $out)"
    exit 1
fi
out=$(TRACKER_HEADLESS=demos/groove.trk timeout 60 ./engine1999c --allow=read tools/tracker.bas 2>&1)
case "$out" in
    *"5 slots, 2 patterns, 160 rows"*"tracker selftest ok 36"*) ;;
    *) echo "FAILED: trackertest (the demo song: $out)"; exit 1 ;;
esac
echo "tracker    ok"
exit 0
