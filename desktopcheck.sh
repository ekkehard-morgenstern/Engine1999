#!/bin/sh
# Checks the desktop integration (desktop/): every installed program (engine1999, engine1999c, each of GAMES and CONGAMES of the makefile)
# has a menu entry desktop/<name>.desktop and an icon desktop/icons/<name>.svg, every entry names an Exec placeholder and an icon that exists,
# and there is no entry or icon for a program that does not exist. If desktop-file-validate / xmllint are installed they check the files too.
# Exit status = number of failures.
cd "$(dirname "$0")" || exit 1
fail=0
bad() { echo "FAILED: $1"; fail=$((fail+1)); }
games=$(sed -n 's/^GAMES=//p;s/^CONGAMES=//p' makefile)
names="engine1999 engine1999c"
for g in $games; do names="$names engine1999-$g"; done
for n in $names; do
    f=desktop/$n.desktop
    [ -f "$f" ] || { bad "no menu entry $f"; continue; }
    [ -f "desktop/icons/$n.svg" ] || bad "no icon desktop/icons/$n.svg"
    grep -q "^Icon=$n\$" "$f" || bad "$f: Icon= is not $n"
    case $n in
        engine1999|engine1999c) want="Exec=@BINDIR@/$n\$" ;;
        *) want="Exec=@GAMESBINDIR@/$n\$" ;;
    esac
    grep -q "^$want" "$f" || bad "$f: Exec= is not $want"
    grep -q '^Categories=.*;$' "$f" || bad "$f: Categories= missing or without a final semicolon"
    grep -q '^Name=' "$f" || bad "$f: no Name="
    grep -q '^Comment=' "$f" || bad "$f: no Comment="
done
for f in desktop/*.desktop; do
    n=$(basename "$f" .desktop)
    case " $names " in *" $n "*) ;; *) bad "$f belongs to no installed program" ;; esac
done
for f in desktop/icons/*.svg; do
    n=$(basename "$f" .svg)
    case " $names " in *" $n "*) ;; *) bad "$f belongs to no installed program" ;; esac
    if command -v xmllint > /dev/null 2>&1; then xmllint --noout "$f" > /dev/null 2>&1 || bad "$f is not well-formed XML"; fi
done
if command -v desktop-file-validate > /dev/null 2>&1; then
    T=$(mktemp -d) || exit 1
    for f in desktop/*.desktop; do
        sed -e 's|@BINDIR@|/usr/bin|' -e 's|@GAMESBINDIR@|/usr/games|' "$f" > "$T/$(basename "$f")"
    done
    desktop-file-validate "$T"/*.desktop > "$T/log" 2>&1 || { bad "desktop-file-validate:"; cat "$T/log"; }
    grep -q . "$T/log" && { echo "desktop-file-validate says:"; cat "$T/log"; fail=$((fail+1)); }
    for f in desktop/*.desktop; do rm -f "$T/$(basename "$f")"; done
    rm -f "$T/log"
    rmdir "$T"
fi
[ $fail -eq 0 ] && echo "desktopcheck: all passed"
exit $fail
