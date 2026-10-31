#!/bin/sh
#
#   Engine1999 - A 2D games engine written in C
#   Copyright (C) 2026  Ekkehard Morgenstern
#
#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <https://www.gnu.org/licenses/>.
#
#   ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
#       applies, see the file LICENSE-EXCEPTION.
#
# Keeps the man pages (man/) honest. "sh mancheck.sh" (make man-check) fails if
#   - an option that "engine1999 --help" or "engine1999c --help" lists is missing from the page of that program,
#   - the list of reserved words in man/engine1999-basic.7 is not the list "engine1999c --list-keywords" prints,
#   - a page has no @VERSION@ in its header, or roff finds a mistake in it (when groff is there).
# "sh mancheck.sh --update" rewrites the list of reserved words (the rest of the pages is written by hand).
# Run it in the build directory (needs ./engine1999 and ./engine1999c).

cd "$(dirname "$0")" || exit 2
page7=man/engine1999-basic.7
failed=0
fail() { echo "FAIL: $*" >&2; failed=$((failed + 1)); }

[ -x ./engine1999c ] || { echo "engine1999c is not built: run make first" >&2; exit 2; }
words=$(./engine1999c --list-keywords | sort -u)

if [ "$1" = "--update" ]; then
    tmp=$(mktemp) || exit 2
    awk -v list="$(printf '%s\n' "$words" | tr '\n' ' ' | fold -s -w 100)" '
        /^\.\\" KEYWORDS-BEGIN/ { print; print list; skip = 1; next }
        /^\.\\" KEYWORDS-END/   { skip = 0 }
        !skip                   { print }' "$page7" > "$tmp" && cat "$tmp" > "$page7"
    rm -f "$tmp"
    echo "reserved words of $page7 rewritten"
    exit 0
fi

# 1. the reserved words
listed=$(awk '/^\.\\" KEYWORDS-BEGIN/ { on = 1; next } /^\.\\" KEYWORDS-END/ { on = 0 } on' "$page7" | tr -s ' \n' '\n\n' | sed '/^$/d' | sort -u)
if [ "$listed" != "$words" ]; then
    fail "the reserved words in $page7 are not those of the language (sh mancheck.sh --update rewrites them)"
    f=$(mktemp) && printf '%s\n' "$listed" > "$f" && printf '%s\n' "$words" | diff - "$f" | head -10 >&2
    rm -f "$f"
fi

# 2. the options
for prog in engine1999 engine1999c; do
    page=man/$prog.1
    [ -x ./$prog ] || { echo "$prog is not built" >&2; exit 2; }
    text=$(sed 's/\\-/-/g; s/\\fB//g; s/\\fR//g; s/\\fI//g' "$page")
    for opt in $(./$prog --help 2>&1 | grep -o -E '^ +(-[a-zA-Z]|--[a-z0-9-]+)(, +--[a-z0-9-]+)?' | grep -o -E -- '--?[a-zA-Z0-9-]+' | sort -u); do
        case "$opt" in -) continue ;; esac
        printf '%s\n' "$text" | grep -q -F -e "$opt" || fail "the option $opt of $prog is not in $page"
    done
done

# 3. form
for page in man/*.1 man/*.7; do
    grep -q '^\.TH .*@VERSION@' "$page" || fail "$page has no @VERSION@ in its .TH line"
    if command -v groff >/dev/null 2>&1; then
        msg=$(sed 's/@VERSION@/x/' "$page" | groff -man -ww -z 2>&1)
        [ -z "$msg" ] || fail "groff has remarks on $page: $(printf '%s' "$msg" | head -3)"
    fi
done

if [ "$failed" -eq 0 ]; then
    echo "man pages ok"
fi
exit "$failed"
