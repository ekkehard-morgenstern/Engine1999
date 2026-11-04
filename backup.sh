#!/bin/bash
# Make a milestone backup of the Engine1999 sources as a ZIP file (nothing goes into git).
#
# Like add-all.sh it picks the files that belong to the project (sources, headers, docs, demos, man pages, scripts, residue); unlike it,
# it takes the working tree as it is, committed or not. The ZIP goes to the directory backups/ and is named after the backup version
# and the time:   backups/engine1999-v0.6.0.3-20261104-153000.zip
#
# The backup version has the form MAJOR.MINOR.BUILD.REVISION and is kept in the file backups/version. MAJOR.MINOR.BUILD is the version
# of the project (the newest version tag of git, see bump_version.sh); REVISION counts the backups made of that version. After a backup the
# revision in backups/version is increased by one. When the project version has changed (a new tag), the file starts again at that version,
# revision 1.
#
# Usage: ./backup.sh [options]
#   --label TEXT   add TEXT (letters, digits, - and _) to the name of the ZIP file:  engine1999-v0.6.0.3-20261104-153000-TEXT.zip
#   --dry-run, -n  only show what would happen (nothing is written)
#   --show         print the version of the next backup and exit
#   --list         list the backups (newest last) and exit
#   --restore ZIP  unpack a backup into backups/restored/<name>/ (never over the working tree) and list what differs from the working
#                  tree; copy back what you want by hand (cp backups/restored/<name>/<file> <file>)
#   --help, -h     this text
#
# Needs the program zip. It never runs a git command that writes (it only asks git for the version tag).

set -euo pipefail

usage() { sed -n '2,/^$/p' "$0" | sed 's/^# \{0,1\}//'; }
die()   { echo "error: $*" >&2; exit 1; }

label=""; dry=0; show=0; list=0; restore=""
while [ $# -gt 0 ]; do
    case "$1" in
        --label)      [ $# -ge 2 ] || die "--label needs a text"; label="$2"; shift ;;
        --dry-run|-n) dry=1 ;;
        --show)       show=1 ;;
        --list)       list=1 ;;
        --restore)    [ $# -ge 2 ] || die "--restore needs a ZIP file"; restore="$2"; shift ;;
        --help|-h)    usage; exit 0 ;;
        *)            usage >&2; die "unknown option: $1" ;;
    esac
    shift
done
[[ -z "$label" || "$label" =~ ^[A-Za-z0-9_-]+$ ]] || die "the label may only have letters, digits, - and _"

cd "$(dirname "$(readlink -f "$0")")"
dir=backups
vfile=$dir/version

if [ "$list" = 1 ]; then
    ls -1 "$dir"/engine1999-v*.zip 2>/dev/null | while read -r f; do printf '%s  %s\n' "$(du -h "$f" | cut -f1)" "$f"; done
    exit 0
fi
if [ -n "$restore" ]; then
    [ -f "$restore" ] || restore="$dir/$restore"
    [ -f "$restore" ] || die "no such backup: $restore"
    command -v unzip >/dev/null 2>&1 || die "the program unzip is not installed"
    dest="$dir/restored/$(basename "$restore" .zip)"
    rm -rf "$dest"; mkdir -p "$dest"
    unzip -q "$restore" -d "$dest" || die "unzip failed"
    echo "unpacked into $dest; differences to the working tree (Only in = missing there):"
    # (only files that differ or exist only in the backup; the build products and the other files of the working tree are left out)
    LC_ALL=C diff -rq "$dest" . 2>/dev/null | sed "s#$dest#BACKUP#" | grep -v -E '^Only in \.(/[^:]*)?: ' | grep -v -E '^Only in \./backups|\.o( |$)' | head -40 || true
    exit 0
fi

# the project version: the newest version tag (vMAJOR.MINOR.BUILD), if git is there
proj=$(git tag --list 'v[0-9]*.[0-9]*.[0-9]*' --sort=version:refname 2>/dev/null | grep -E '^v[0-9]+\.[0-9]+\.[0-9]+$' | tail -n 1 || true)
proj=${proj#v}

# the backup version from the file (MAJOR.MINOR.BUILD.REVISION)
cur=""
[ -f "$vfile" ] && cur=$(tr -d ' \t\r\n' < "$vfile")
if [[ ! "$cur" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
    [ -z "$cur" ] || echo "note: $vfile does not hold MAJOR.MINOR.BUILD.REVISION ('$cur'): starting again" >&2
    cur="${proj:-0.0.0}.1"
fi
IFS=. read -r major minor build rev <<<"$cur"
base="$major.$minor.$build"
if [ -n "$proj" ] && [ "$proj" != "$base" ]; then
    echo "note: the project version is v$proj (the backup version file said $base): starting at revision 1" >&2
    base=$proj; rev=1
fi
rev=$((10#$rev))
ver="$base.$rev"

if [ "$show" = 1 ]; then echo "v$ver"; exit 0; fi

stamp=$(date +%Y%m%d-%H%M%S)
name="engine1999-v$ver-$stamp${label:+-$label}.zip"
out="$dir/$name"

# what goes in: the patterns of add-all.sh, plus what it leaves out because git tracks it by other means (demos/lib, scripts, text files,
# the licenses, the version file of the backups); no build products, no zip files
shopt -s nullglob
files=( *.md *.c *.h *.ebnf *.pl *.txt .gitignore makefile LICENSE LICENSE-EXCEPTION *.sh
        demos/* demos/lib/* docs/*.md docs/*.html docs/images/* man/* residue/* tools/* "$vfile" )
# drop directories (zip would take their contents) and anything that is not a file
keep=()
for f in "${files[@]}"; do [ -f "$f" ] && keep+=( "$f" ); done
[ "${#keep[@]}" -gt 0 ] || die "no files found: run this in the Engine1999 source directory"

command -v zip >/dev/null 2>&1 || die "the program zip is not installed"
echo "backup version:  v$ver"
echo "file:            $out  (${#keep[@]} files)"
if [ "$dry" = 1 ]; then echo "dry run: nothing written"; exit 0; fi

mkdir -p "$dir"
[ ! -e "$out" ] || die "$out exists already"
zip -q -X "$out" "${keep[@]}" || { rm -f "$out"; die "zip failed"; }
# a quick check that the archive is sound (zip -T needs unzip: skipped without it)
if command -v unzip >/dev/null 2>&1; then
    zip -q -T "$out" || die "the archive $out is damaged"
fi

# only now count up (a failed backup does not use a revision)
IFS=. read -r M m b <<<"$base"
printf '%s.%s.%s.%s\n' "$M" "$m" "$b" "$((rev + 1))" > "$vfile"
echo "done:            $(du -h "$out" | cut -f1)  next backup: v$M.$m.$b.$((rev + 1))"
