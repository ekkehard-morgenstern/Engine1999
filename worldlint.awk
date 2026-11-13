# worldlint.awk - the world sanity check (docs/world-model.md "Walls have depth"): reads a map file (W3DMAP, written by WM_SAVE of demos/lib/w3dmap.lib after
# W3D WORLD) and reports every pair of solid walls in different sectors that lie on the same plane with their faces turned away from each other and
# overlap in height: a wall of zero depth that shows a face on each side. (Walls with depth are separate rooms, or a gap with nothing in it.)
# It also checks that every portal leads somewhere: the point just beyond a portal wall (7 samples along it) must lie inside the sector the portal opens into
# (even-odd rule over that sector's walls, holes included). A ray that enters a sector at a place it does not cover is lost: the picture shows a black column.
# Use: awk -f worldlint.awk file.map  (prints the faults, exit status 1 if there are any; only axis-parallel walls are looked at for the first check)
BEGIN { FS = ","; n = 0; bad = 0 }
$1 == "SECTOR" { fl[$2] = $3; ce[$2] = $4; next }
$1 == "WALL" && $8 == -1 {
    n++; s[n] = $2; x1[n] = $3; y1[n] = $4; x2[n] = $5; y2[n] = $6; t[n] = $7
}
$1 == "WALL" {
    m++; ws[m] = $2; wx1[m] = $3; wy1[m] = $4; wx2[m] = $5; wy2[m] = $6; wnx[m] = $8
    cnt[$2]++; wi[$2, cnt[$2]] = m
}
function inside( sec, px, py,    k, i, c, xi ) {
    c = 0
    for ( k = 1; k <= cnt[sec]; k++ ) {
        i = wi[sec, k]
        if ( ( wy1[i] > py ) != ( wy2[i] > py ) ) {
            xi = wx1[i] + ( py - wy1[i] ) * ( wx2[i] - wx1[i] ) / ( wy2[i] - wy1[i] )
            if ( xi > px ) c = !c
        }
    }
    return c
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
    for ( i = 1; i <= m; i++ ) {
        if ( wnx[i] < 0 ) continue
        dx = wx2[i] - wx1[i]; dy = wy2[i] - wy1[i]; ln = sqrt( dx * dx + dy * dy )
        if ( ln == 0 ) continue
        for ( q = 1; q <= 7; q++ ) {
            f = q / 8
            px = wx1[i] + dx * f + dy / ln * 0.5; py = wy1[i] + dy * f - dx / ln * 0.5
            if ( !inside( wnx[i], px, py ) ) {
                printf "lost portal: sector %d to %d at %d,%d .. %d,%d: the far side is not inside sector %d\n", ws[i], wnx[i], wx1[i], wy1[i], wx2[i], wy2[i], wnx[i]
                bad++; break
            }
        }
    }
    exit bad > 0
}
