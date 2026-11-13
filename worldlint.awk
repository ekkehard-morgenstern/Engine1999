# worldlint.awk - the world sanity check (docs/world-model.md "Walls have depth"): reads a map file (W3DMAP, written by WM_SAVE of demos/lib/w3dmap.lib after
# W3D WORLD) and reports every pair of solid walls in different sectors that lie on the same plane with their faces turned away from each other and
# overlap in height: a wall of zero depth that shows a face on each side. (Walls with depth are separate rooms, or a gap with nothing in it.)
# Use: awk -f worldlint.awk file.map  (prints the pairs, exit status 1 if there are any; only axis-parallel walls are looked at)
BEGIN { FS = ","; n = 0; bad = 0 }
$1 == "SECTOR" { fl[$2] = $3; ce[$2] = $4; next }
$1 == "WALL" && $8 == -1 {
    n++; s[n] = $2; x1[n] = $3; y1[n] = $4; x2[n] = $5; y2[n] = $6; t[n] = $7
}
END {
    for (i = 1; i <= n; i++) for (j = i + 1; j <= n; j++) {
        if (s[i] == s[j]) continue
        if (y1[i] == y2[i] && y1[j] == y2[j] && y1[i] == y1[j] && x1[i] != x2[i] && x1[j] != x2[j]) {
            if ((x2[i] - x1[i]) * (x2[j] - x1[j]) >= 0) continue
            lo = (x1[i] < x2[i] ? x1[i] : x2[i]); l2 = (x1[j] < x2[j] ? x1[j] : x2[j]); if (l2 > lo) lo = l2
            hi = (x1[i] > x2[i] ? x1[i] : x2[i]); h2 = (x1[j] > x2[j] ? x1[j] : x2[j]); if (h2 < hi) hi = h2
        } else if (x1[i] == x2[i] && x1[j] == x2[j] && x1[i] == x1[j] && y1[i] != y2[i] && y1[j] != y2[j]) {
            if ((y2[i] - y1[i]) * (y2[j] - y1[j]) >= 0) continue
            lo = (y1[i] < y2[i] ? y1[i] : y2[i]); l2 = (y1[j] < y2[j] ? y1[j] : y2[j]); if (l2 > lo) lo = l2
            hi = (y1[i] > y2[i] ? y1[i] : y2[i]); h2 = (y1[j] > y2[j] ? y1[j] : y2[j]); if (h2 < hi) hi = h2
        } else continue
        if (hi <= lo) continue
        z0 = (fl[s[i]] > fl[s[j]] ? fl[s[i]] : fl[s[j]]); z1 = (ce[s[i]] < ce[s[j]] ? ce[s[i]] : ce[s[j]])
        if (z1 <= z0) continue
        printf "zero-depth wall: sectors %d and %d, %d .. %d, height %d .. %d, pictures %d / %d\n", s[i], s[j], lo, hi, z0, z1, t[i], t[j]
        bad++
    }
    exit bad > 0
}
