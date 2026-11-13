#!/bin/sh
# Test of the generators in demos/levels (written in the built-in BASIC, run by engine1999c): the DATA lines / PLAY procedures they write must be the ones
# that the demos contain (the output was pasted into demos/manor.bas, demos/bughunt.bas, demos/deqq.bas, demos/lib/deqqart.lib and demos/lib/music3d.lib).
# For each generator the output has to appear as one unbroken run of lines in the target file (line numbers and surrounding blanks ignored).
# Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
T=$(mktemp -d) || exit 1
trap 'rm -rf "$T"' EXIT
fail=0
# contains OUTPUT TARGET: the lines of OUTPUT are a contiguous run in TARGET
contains() {
    awk -v gfile="$1" '
        function trim(s) { sub(/^[ \t]+/, "", s); sub(/[ \t\r]+$/, "", s); return s }
        BEGIN { while ((getline l < gfile) > 0) g[++ng] = trim(l) }
        { l = $0; sub(/^[0-9]+[ \t]/, "", l); d[++nd] = trim(l) }
        END {
            for (i = 1; i + ng - 1 <= nd; i++) {
                if (d[i] != g[1]) continue
                ok = 1
                for (j = 2; j <= ng; j++) if (d[i + j - 1] != g[j]) { ok = 0; break }
                if (ok) exit 0
            }
            exit 1
        }' "$2"
}
check() {   # check name program target
    if ! timeout 120 ./engine1999c "$2" > "$T/out" 2> "$T/err"; then echo "FAILED: $1 (the program failed: $(head -n 2 "$T/err"))"; fail=$((fail+1)); return; fi
    if contains "$T/out" "$3"; then printf '%-10s ok\n' "$1"; else echo "FAILED: $1 (output is not in $3)"; fail=$((fail+1)); fi
}
check manor demos/levels/manor.bas demos/manor.bas
check bunker demos/levels/bunker.bas demos/bughunt.bas
check deqq2art demos/levels/deqq2art.bas demos/lib/deqqart.lib
check deqq1 demos/levels/deqq1.bas demos/deqq.bas
# the showroom generator writes quoted DATA lines for the map when it is given "-" and "data"
if timeout 120 ./engine1999c demos/levels/showroom.bas - data > "$T/out" 2> "$T/err" && contains "$T/out" demos/showroom.bas; then printf '%-10s ok\n' showroom; else echo "FAILED: showroom (output is not in demos/showroom.bas)"; fail=$((fail+1)); fi
# the music generator also prints a report on stdout: its file argument is a file in the generator's folder
cp demos/levels/music3d.bas "$T/music3d.bas"
( cd "$T" && timeout 120 "$OLDPWD/engine1999c" music3d.bas out.txt > report 2> err ) || { echo "FAILED: music3d (the program failed)"; fail=$((fail+1)); }
if contains "$T/out.txt" demos/lib/music3d.lib; then printf '%-10s ok\n' music3d; else echo "FAILED: music3d (output is not in demos/lib/music3d.lib)"; fail=$((fail+1)); fi
exit $fail
