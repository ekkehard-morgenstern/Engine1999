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
#       applies. Programs written in the built-in programming language, and
#       executables made from them with BUILD, do not fall under this license
#       as the exception describes (see the file LICENSE-EXCEPTION).
#
#   CONTACT INFO:
#       E-Mail: ekkehard@ekkehardmorgenstern.de
#       Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
#             Germany, Europe

# Optional: the settings that ./configure found (prefix, compiler, SDL2 flags); everything works without it, too
-include config.mk

SDL_CFLAGS?=
SDL_LIBS?=-lSDL2

# The compiler: gcc, unless the caller says CC=... (the internal compile command is $(CCO), not CC)
ifeq ($(origin CC),default)
CCOMP=gcc
else
CCOMP=$(CC)
endif

ifdef DEBUG
DBGFLG=-g
else
DBGFLG=-O3
endif

# PROFILE=1 adds -p (every run writes gmon.out); off by default.
ifdef PROFILE
PRFFLG=-p
endif

# Flags: CFLAGS, CPPFLAGS and LDFLAGS from the environment or the command line (as a package build passes them, e.g. from dpkg-buildflags)
# replace the development defaults, and the installed kit (make kit) then uses them too. Without them: the development build (-Werror, -march=native)
# and a portable -O2 kit.
ifeq ($(filter environment command line file,$(firstword $(origin CFLAGS))),)
CFLAGS=-Wall -Werror $(PRFFLG) $(DBGFLG) -march=native -mtune=native
KITFLAGS=-Wall -Werror -O2
else
KITFLAGS=$(CFLAGS)
endif

CCO=$(CCOMP) -c $(SDL_CFLAGS) $(CPPFLAGS) $(CFLAGS)
CL=$(CCOMP) $(CFLAGS) $(LDFLAGS)

.c.o:
	$(CCO) -o $@ $<

PROGS=sdltest1 sdltest2 sdltest3 sdltest4 engine1999 joytest1 basictest1 testcomp1 basictest2 vmtest1 repltest1 ansitest1 txttest1 asmtest1 synthtest1 iotest1 gfxtest1 buildtest1 engine1999c contest1 keytest1 scrtest1 libtest1 fonttest1 uitest1 synhltest1 poltest1 w3dtest1

all: $(PROGS)
	echo ok >all

.PHONY: grammar grammar-check kit install uninstall man-check check hosttest dist orig clean distclean

# the quick reference of the language (docs/grammar.md and docs/grammar.html) from bascomp.ebnf; needs perl
grammar:
	perl grammar.pl

# fails if a rule is used but not defined, or if the quick reference is out of date
grammar-check:
	perl grammar.pl --check

BASEMOD=sdlevent.o sdllayer.o sdlmain.o sdlscreen.o textscreen.o tilescreen.o sprscreen.o sprpath.o policy.o 8x12font1.o sdlaudio.o synth.o mml.o printusing.o clipboard.o sdlutil.o \
		basic.o baspgm.o baslin.o bastok.o bascomp.o nodetype.o codegen.o cgenctl.o cgendecl.o cgenprog.o vars.o runtime.o vm.o basfiles.o iodev.o gfxcore.o gfxops.o w3d.o bmp.o gfxfile.o basbuild.o baslib.o cgenlib.o basrepl.o basdebug.o ansi.o palette.o keyseq.o keystate.o cursors.o mouse.o joy.o sdljoy.o sdlkeys.o scredit.o conscr.o fulledit.o vmasm.o monitor.o font.o ui.o synhl.o
BASEHDR=sdlevent.h sdllayer.h sdlmain.h sdlscreen.h sdltypes.h textscreen.h tilescreen.h sprscreen.h sprpath.h policy.h unxtypes.h sdlaudio.h synth.h mml.h printusing.h clipboard.h sdlutil.h basfiles.h iodev.h gfxcore.h gfxops.h w3d.h bmp.h gfxfile.h basbuild.h baslib.h basic.h baspgm.h baslin.h bastok.h bascomp.h nodetype.h codegen.h instset.h vars.h runtime.h vm.h basrepl.h basdebug.h ansi.h palette.h keyseq.h keystate.h cursors.h mouse.h joy.h sdljoy.h sdlkeys.h scrdrv.h conscr.h fulledit.h vmasm.h monitor.h font.h ui.h synhl.h

LIBMOD=$(BASEMOD)
LIBHDR=$(BASEHDR)
LIBNAME=engine.lib

sdltest1: sdltest1.o $(LIBNAME)
	$(CL) -o sdltest1 sdltest1.o $(LIBNAME) $(SDL_LIBS) -lrt -lm

sdltest1.o:	sdltest1.c $(LIBHDR)

sdltest2: sdltest2.o $(LIBNAME)
	$(CL) -o sdltest2 sdltest2.o $(LIBNAME) $(SDL_LIBS) -lrt -lm

sdltest2.o:	sdltest2.c $(LIBHDR)

sdltest3: sdltest3.o $(LIBNAME)
	$(CL) -o sdltest3 sdltest3.o $(LIBNAME) $(SDL_LIBS) -lrt -lm

sdltest3.o:	sdltest3.c $(LIBHDR)

sdltest4: sdltest4.o $(LIBNAME)
	$(CL) -o sdltest4 sdltest4.o $(LIBNAME) $(SDL_LIBS) -lrt -lm

sdltest4.o:	sdltest4.c $(LIBHDR)

engine1999: engine1999.o $(LIBNAME)
	$(CL) -o engine1999 engine1999.o $(LIBNAME) $(SDL_LIBS) -lrt -lm

engine1999.o: engine1999.c $(LIBHDR) version.h

# the console host: the same BASIC system without a window, no SDL (docs/console-host.md)
engine1999c: engine1999c.o $(LIBNAME)
	$(CL) -o engine1999c engine1999c.o $(LIBNAME) -lrt -lm

engine1999c.o: engine1999c.c $(LIBHDR) version.h

# the version string comes from the newest version tag of git (bump_version.sh makes the tags); the header is regenerated when HEAD,
# the log of commits or the tags change (and only rewritten if the string differs). Only engine1999.o includes it.
version.h: tagtoverstr.sh $(wildcard .git/HEAD .git/logs/HEAD .git/refs/tags .git/packed-refs)
	sh tagtoverstr.sh version.h

basictest1: basictest1.o $(LIBNAME)
	$(CL) -o basictest1 basictest1.o $(LIBNAME) -lm

basictest1.o: basictest1.c $(LIBHDR)

basictest2: basictest2.o $(LIBNAME)
	$(CL) -o basictest2 basictest2.o $(LIBNAME) -lm

basictest2.o: basictest2.c $(LIBHDR)

vmtest1: vmtest1.o $(LIBNAME)
	$(CL) -o vmtest1 vmtest1.o $(LIBNAME) -lm

vmtest1.o: vmtest1.c $(LIBHDR)

repltest1: repltest1.o $(LIBNAME)
	$(CL) -o repltest1 repltest1.o $(LIBNAME) -lm

repltest1.o: repltest1.c $(LIBHDR)

ansitest1: ansitest1.o $(LIBNAME)
	$(CL) -o ansitest1 ansitest1.o $(LIBNAME) -lm

ansitest1.o: ansitest1.c $(LIBHDR)

joytest1: joytest1.o $(LIBNAME)
	$(CL) -o joytest1 joytest1.o $(LIBNAME) -lm

joytest1.o: joytest1.c $(LIBHDR)

keytest1: keytest1.o $(LIBNAME)
	$(CL) -o keytest1 keytest1.o $(LIBNAME) -lm

keytest1.o: keytest1.c $(LIBHDR)

scrtest1: scrtest1.o $(LIBNAME)
	$(CL) -o scrtest1 scrtest1.o $(LIBNAME) -lm

scrtest1.o: scrtest1.c $(LIBHDR)

txttest1: txttest1.o $(LIBNAME)
	$(CL) -o txttest1 txttest1.o $(LIBNAME) -lm

txttest1.o: txttest1.c $(LIBHDR)

iotest1: iotest1.o $(LIBNAME)
	$(CL) -o iotest1 iotest1.o $(LIBNAME) -lm

iotest1.o: iotest1.c $(LIBHDR)

gfxtest1: gfxtest1.o $(LIBNAME)
	$(CL) -o gfxtest1 gfxtest1.o $(LIBNAME) -lm

libtest1: libtest1.o $(LIBNAME)
	$(CL) -o libtest1 libtest1.o $(LIBNAME) -lm

libtest1.o: libtest1.c $(LIBHDR)

poltest1: poltest1.o $(LIBNAME)
	$(CL) -o poltest1 poltest1.o $(LIBNAME) -lm

poltest1.o: poltest1.c $(LIBHDR)

fonttest1: fonttest1.o $(LIBNAME)
	$(CL) -o fonttest1 fonttest1.o $(LIBNAME) -lm

fonttest1.o: fonttest1.c $(LIBHDR)

uitest1: uitest1.o $(LIBNAME)
	$(CL) -o uitest1 uitest1.o $(LIBNAME) -lm

uitest1.o: uitest1.c $(LIBHDR) ui.h

ui.o: ui.c ui.h font.h gfxcore.h synhl.h

synhltest1: synhltest1.o $(LIBNAME)
	$(CL) -o synhltest1 synhltest1.o $(LIBNAME) -lm

synhltest1.o: synhltest1.c $(LIBHDR) synhl.h

synhl.o: synhl.c synhl.h $(BASEHDR)

gfxtest1.o: gfxtest1.c $(LIBHDR)

# the stand-in runtime of buildtest1 (a main() that writes out the embedded program); not part of the library
buildstub.o: buildstub.c $(LIBHDR)

# buildtest1 needs engine1999.o next to it (the real runtime kit: engine1999.o + engine.lib) and runs its tests in this directory
buildtest1: buildtest1.o buildstub.o engine1999.o engine1999c.o $(LIBNAME)
	$(CL) -o buildtest1 buildtest1.o $(LIBNAME) -lm

buildtest1.o: buildtest1.c $(LIBHDR)

basbuild.o: basbuild.c $(BASEHDR)

# the test of the console host runs ./engine1999c in the build directory
contest1: contest1.o engine1999c engine1999.o
	$(CL) -o contest1 contest1.o

contest1.o: contest1.c $(LIBHDR)

asmtest1: asmtest1.o $(LIBNAME)
	$(CL) -o asmtest1 asmtest1.o $(LIBNAME) -lm

asmtest1.o: asmtest1.c $(LIBHDR)

synthtest1: synthtest1.o $(LIBNAME)
	$(CL) -o synthtest1 synthtest1.o $(LIBNAME) -lm

synthtest1.o: synthtest1.c $(LIBHDR)

testcomp1: testcomp1.o $(LIBNAME)
	$(CL) -o testcomp1 testcomp1.o $(LIBNAME) -lrt -lm

testcomp1.o: testcomp1.c $(LIBHDR)

$(LIBNAME): $(LIBMOD)
	-rm $(LIBNAME)
	ar q $(LIBNAME) $(LIBMOD)

# -- the installed kit (make install) ---------------------------------------------------------
#
# What BUILD needs (engine1999.o, engine1999c.o, engine.lib) and the two programs, compiled for installing: portable (no -march=native, which would make
# the executables run only on CPUs like this one) and without the profiling hooks (-p) of the development build. They go to kit/, the development
# objects stay as they are. Everything is made with "make kit"; "make install" copies it (PREFIX=/usr/local by default, DESTDIR for packagers).

PREFIX?=/usr/local
BINDIR?=$(PREFIX)/bin
# where the games go (a package build uses $(PREFIX)/games, the Debian place for games)
GAMESBINDIR?=$(BINDIR)
LIBDIR=$(PREFIX)/lib/engine1999
SHAREDIR=$(PREFIX)/share/engine1999
DOCDIR=$(PREFIX)/share/doc/engine1999
MANDIR?=$(PREFIX)/share/man
# desktop integration (desktop/): the menu entries and the icons (SVG, the freedesktop.org icon theme "hicolor")
APPDIR?=$(PREFIX)/share/applications
ICONDIR?=$(PREFIX)/share/icons/hicolor/scalable/apps
KITOBJ=$(addprefix kit/,$(BASEMOD))

kit: kit/engine1999 kit/engine1999c kit/engine1999.o kit/engine1999c.o kit/engine.lib

# The demo games as programs of their own (roadmap item 60): each demo is turned into an executable with BUILD (window kind, its libraries
# inside), named engine1999-<game> so that nothing on the user's system clashes. Needs only the system linker, like BUILD itself.
GAMES=orchard orchard2 orchard3 climb climb2 madrun madrun2 river river2 rustinpeace glyphminer y2k deqq bughunt deqq2
GAMEEXE=$(addprefix kit/games/engine1999-,$(GAMES))
# text games: console executables (BUILD ..., CONSOLE: no window, no SDL2 needed to run them)
CONGAMES=wumpus
CONEXE=$(addprefix kit/games/engine1999-,$(CONGAMES))

games: $(GAMEEXE) $(CONEXE)

$(GAMEEXE): kit/games/engine1999-%: demos/%.bas $(wildcard demos/lib/*.lib) kit/engine1999c kit/engine1999.o kit/engine.lib
	@mkdir -p kit/games
	printf 'LOAD "demos/$*"\nBUILD "$@", WINDOW\nBYE\n' | ENGINE1999_HOME=$(CURDIR)/kit ./kit/engine1999c
	@test -x $@

$(CONEXE): kit/games/engine1999-%: demos/%.bas $(wildcard demos/lib/*.lib) kit/engine1999c kit/engine1999c.o kit/engine.lib
	@mkdir -p kit/games
	printf 'LOAD "demos/$*"\nBUILD "$@", CONSOLE\nBYE\n' | ENGINE1999_HOME=$(CURDIR)/kit ./kit/engine1999c
	@test -x $@

$(KITOBJ) kit/engine1999.o kit/engine1999c.o: kit/%.o: %.c $(BASEHDR)
	@mkdir -p kit
	$(CCOMP) -c $(SDL_CFLAGS) $(CPPFLAGS) $(KITFLAGS) -o $@ $<

kit/engine1999.o kit/engine1999c.o: version.h

kit/engine.lib: $(KITOBJ)
	-rm -f kit/engine.lib
	ar q kit/engine.lib $(KITOBJ)

kit/engine1999: kit/engine1999.o kit/engine.lib
	$(CCOMP) $(KITFLAGS) $(LDFLAGS) -o $@ kit/engine1999.o kit/engine.lib $(SDL_LIBS) -lrt -lm

kit/engine1999c: kit/engine1999c.o kit/engine.lib
	$(CCOMP) $(KITFLAGS) $(LDFLAGS) -o $@ kit/engine1999c.o kit/engine.lib -lrt -lm

install: kit games version.h
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(GAMESBINDIR) $(DESTDIR)$(LIBDIR)/lib $(DESTDIR)$(LIBDIR)/tools $(DESTDIR)$(SHAREDIR)/demos/lib $(DESTDIR)$(DOCDIR)/docs/images $(DESTDIR)$(DOCDIR)/docs/roadmap \
		$(DESTDIR)$(MANDIR)/man1 $(DESTDIR)$(MANDIR)/man6 $(DESTDIR)$(MANDIR)/man7 $(DESTDIR)$(APPDIR) $(DESTDIR)$(ICONDIR)
	install -m 755 kit/engine1999 kit/engine1999c $(DESTDIR)$(BINDIR)
	install -m 755 $(GAMEEXE) $(CONEXE) $(DESTDIR)$(GAMESBINDIR)
	install -m 644 kit/engine1999.o kit/engine1999c.o kit/engine.lib $(DESTDIR)$(LIBDIR)
	install -m 644 demos/*.bas demos/README.md $(DESTDIR)$(SHAREDIR)/demos
	install -m 644 demos/lib/*.lib $(DESTDIR)$(SHAREDIR)/demos/lib
	install -m 644 demos/lib/*.lib $(DESTDIR)$(LIBDIR)/lib
	install -m 644 tools/*.bas $(DESTDIR)$(LIBDIR)/tools
	install -m 644 README.md LICENSE LICENSE-EXCEPTION $(DESTDIR)$(DOCDIR)
	install -m 644 docs/*.md docs/grammar.html $(DESTDIR)$(DOCDIR)/docs
	install -m 644 docs/roadmap/*.md $(DESTDIR)$(DOCDIR)/docs/roadmap
	install -m 644 docs/images/*.png $(DESTDIR)$(DOCDIR)/docs/images
	install -m 644 desktop/icons/*.svg $(DESTDIR)$(ICONDIR)
	for d in desktop/*.desktop; do sed -e 's|@BINDIR@|$(BINDIR)|' -e 's|@GAMESBINDIR@|$(GAMESBINDIR)|' $$d > $(DESTDIR)$(APPDIR)/$$(basename $$d); chmod 644 $(DESTDIR)$(APPDIR)/$$(basename $$d); done
	@v=$$(sed -n 's/.*ENGINE_VERSION *"\(.*\)".*/\1/p' version.h); \
	for p in man/*.1; do sed "s/@VERSION@/$$v/" $$p > $(DESTDIR)$(MANDIR)/man1/$$(basename $$p); chmod 644 $(DESTDIR)$(MANDIR)/man1/$$(basename $$p); done; \
	for p in man/*.6; do sed "s/@VERSION@/$$v/" $$p > $(DESTDIR)$(MANDIR)/man6/$$(basename $$p); chmod 644 $(DESTDIR)$(MANDIR)/man6/$$(basename $$p); done; \
	for p in man/*.7; do sed "s/@VERSION@/$$v/" $$p > $(DESTDIR)$(MANDIR)/man7/$$(basename $$p); chmod 644 $(DESTDIR)$(MANDIR)/man7/$$(basename $$p); done
	@echo "installed in $(DESTDIR)$(PREFIX): engine1999, engine1999c, the games ($(addprefix engine1999-,$(GAMES) $(CONGAMES))), the BUILD kit in $(DESTDIR)$(LIBDIR), man pages (man engine1999), menu entries and icons"
	@if [ -z "$(DESTDIR)" ]; then \
		(update-desktop-database -q $(APPDIR) || true) 2>/dev/null; \
		(gtk-update-icon-cache -q -t -f $(PREFIX)/share/icons/hicolor || true) 2>/dev/null; \
	fi

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/engine1999 $(DESTDIR)$(BINDIR)/engine1999c $(addprefix $(DESTDIR)$(GAMESBINDIR)/engine1999-,$(GAMES) $(CONGAMES))
	rm -rf $(DESTDIR)$(LIBDIR) $(DESTDIR)$(SHAREDIR) $(DESTDIR)$(DOCDIR)
	for p in man/*.1; do rm -f $(DESTDIR)$(MANDIR)/man1/$$(basename $$p); done
	for p in man/*.6; do rm -f $(DESTDIR)$(MANDIR)/man6/$$(basename $$p); done
	for p in man/*.7; do rm -f $(DESTDIR)$(MANDIR)/man7/$$(basename $$p); done
	for p in desktop/*.desktop; do rm -f $(DESTDIR)$(APPDIR)/$$(basename $$p); done
	for p in desktop/icons/*.svg; do rm -f $(DESTDIR)$(ICONDIR)/$$(basename $$p); done

# the man pages (man/) must say what the programs do: options, reserved words (needs ./engine1999 and ./engine1999c)
man-check: engine1999 engine1999c
	sh mancheck.sh

sdlevent.o: sdlevent.c $(BASEHDR)

sdllayer.o: sdllayer.c $(BASEHDR)

sdlmain.o: sdlmain.c $(BASEHDR)

sdlscreen.o: sdlscreen.c $(BASEHDR)

textscreen.o: textscreen.c $(BASEHDR)

tilescreen.o: tilescreen.c $(BASEHDR)

sprscreen.o: sprscreen.c $(BASEHDR)

8x12font.o: 8x12font.c $(BASEHDR)

sdlaudio.o: sdlaudio.c $(BASEHDR)

synth.o: synth.c $(BASEHDR)

sprpath.o: sprpath.c $(BASEHDR)

policy.o: policy.c $(BASEHDR)

mml.o: mml.c $(BASEHDR)

printusing.o: printusing.c $(BASEHDR)

clipboard.o: clipboard.c $(BASEHDR)

sdlutil.o: sdlutil.c $(BASEHDR)

basic.o: basic.c $(BASEHDR)

baspgm.o: baspgm.c $(BASEHDR)

baslin.o: baslin.c $(BASEHDR)

bastok.o: bastok.c $(BASEHDR)

bascomp.o: bascomp.c $(BASEHDR)

nodetype.o: nodetype.c $(BASEHDR)

codegen.o: codegen.c $(BASEHDR)

cgenctl.o: cgenctl.c $(BASEHDR)

cgendecl.o: cgendecl.c $(BASEHDR)

cgenprog.o: cgenprog.c $(BASEHDR)

vars.o: vars.c $(BASEHDR)

runtime.o: runtime.c $(BASEHDR)

vm.o: vm.c $(BASEHDR)

basfiles.o: basfiles.c $(BASEHDR)

baslib.o: baslib.c $(BASEHDR)

cgenlib.o: cgenlib.c $(BASEHDR)

iodev.o: iodev.c $(BASEHDR)

gfxcore.o: gfxcore.c $(BASEHDR)

gfxops.o: gfxops.c $(BASEHDR)

w3d.o: w3d.c $(BASEHDR)

w3dtest1: w3dtest1.o $(LIBNAME)
	$(CL) -o w3dtest1 w3dtest1.o $(LIBNAME) -lm

w3dtest1.o: w3dtest1.c $(LIBHDR)

bmp.o: bmp.c $(BASEHDR)

gfxfile.o: gfxfile.c $(BASEHDR)

basrepl.o: basrepl.c $(BASEHDR)

basdebug.o: basdebug.c $(BASEHDR)

palette.o: palette.c $(BASEHDR)

conscr.o: conscr.c $(BASEHDR)

scredit.o: scredit.c $(BASEHDR)

keyseq.o: keyseq.c $(BASEHDR)

keystate.o: keystate.c $(BASEHDR)

cursors.o: cursors.c $(BASEHDR)
mouse.o: mouse.c $(BASEHDR)

joy.o: joy.c $(BASEHDR)

sdljoy.o: sdljoy.c $(BASEHDR)

sdlkeys.o: sdlkeys.c $(BASEHDR)

ansi.o: ansi.c $(BASEHDR)

fulledit.o: fulledit.c $(BASEHDR)

vmasm.o: vmasm.c $(BASEHDR)

monitor.o: monitor.c $(BASEHDR)

# -- check and dist (packaging, roadmap item 66) ---------------------------------------------------------
# make check: all automated tests (the programs of CHECKTESTS, no display needed) and the man pages check; prints one line per test, the output of
# a failing one, and fails if any failed. Run it in the build directory.
CHECKTESTS=poltest1 vmtest1 repltest1 ansitest1 txttest1 asmtest1 synthtest1 iotest1 gfxtest1 buildtest1 contest1 keytest1 scrtest1 libtest1 uitest1 synhltest1 fonttest1 joytest1 w3dtest1

check: $(CHECKTESTS) engine1999 engine1999c
	@fail=0; log=$$(mktemp); \
	for t in $(CHECKTESTS); do \
		if ./$$t > $$log 2>&1; then printf '%-12s ok\n' $$t; else printf '%-12s FAILED\n' $$t; tail -n 20 $$log; fail=$$((fail+1)); fi; \
	done; \
	if sh mancheck.sh > $$log 2>&1; then printf '%-12s ok\n' man-check; else printf '%-12s FAILED\n' man-check; tail -n 20 $$log; fail=$$((fail+1)); fi; \
	if sh desktopcheck.sh > $$log 2>&1; then printf '%-12s ok\n' desktopcheck; else printf '%-12s FAILED\n' desktopcheck; tail -n 20 $$log; fail=$$((fail+1)); fi; \
	if sh deqqtest.sh > $$log 2>&1; then printf '%-12s ok\n' deqqtest; else printf '%-12s FAILED\n' deqqtest; tail -n 20 $$log; fail=$$((fail+1)); fi; \
	if sh deqq2test.sh > $$log 2>&1; then printf '%-12s ok\n' deqq2test; else printf '%-12s FAILED\n' deqq2test; tail -n 20 $$log; fail=$$((fail+1)); fi; \
	if sh levelstest.sh > $$log 2>&1; then printf '%-12s ok\n' levelstest; else printf '%-12s FAILED\n' levelstest; tail -n 20 $$log; fail=$$((fail+1)); fi; \
	if sh instedtest.sh > $$log 2>&1; then printf '%-12s ok\n' instedtest; else printf '%-12s FAILED\n' instedtest; tail -n 20 $$log; fail=$$((fail+1)); fi; \
	rm -f $$log; \
	if [ $$fail -ne 0 ]; then echo "$$fail check(s) failed"; exit 1; fi; echo "all checks passed"

# make hosttest: the window host's sound path with Xvfb, XTest typing (xtype.c) and SDL's disk audio driver (hosttest.sh); skipped if the X tools are missing.
# Not part of "make check" (needs an X server program).
hosttest: engine1999
	sh hosttest.sh

# make dist: engine1999-<version>.tar.gz with the files git tracks (but not the owner's backup and scratch folders) and the generated version.h,
# so that the archive builds without git (tagtoverstr.sh keeps an existing version.h). Needs a git checkout; read-only git only.
dist: version.h
	@v=$$(sed -n 's/.*ENGINE_VERSION *"v\{0,1\}\([^"]*\)".*/\1/p' version.h); d=engine1999-$$v; \
	list=$$(mktemp); (git ls-files | grep -v '^backups/\|^residue/'; echo version.h) > $$list; \
	tar --transform "s,^,$$d/," -czf $$d.tar.gz -T $$list && rm -f $$list && echo "made $$d.tar.gz"

# make orig: engine1999_<version>.orig.tar.gz, the upstream tarball of a Debian source package in the quilt format (debian/source/format): the
# same files as "make dist" but without debian/, in the directory engine1999-<version>/; put it next to the source directory and build with
# dpkg-buildpackage. The version is the one of the newest git tag (a version after a tag, like 0.6.19-3-g1a2b3c4, becomes 0.6.19+3.g1a2b3c4);
# ORIGVER=x.y.z overrides it (it must match debian/changelog). Release: tag the commit, make orig, dpkg-buildpackage -S.
orig: version.h
	@v=$(ORIGVER); [ -n "$$v" ] || v=$$(sed -n 's/.*ENGINE_VERSION *"v\{0,1\}\([^"]*\)".*/\1/p' version.h | sed 's/-\([0-9][0-9]*\)-g\([0-9a-f][0-9a-f]*\)$$/+\1.g\2/'); \
	d=engine1999-$$v; list=$$(mktemp); (git ls-files | grep -v '^backups/\|^residue/\|^debian/'; echo version.h) > $$list; \
	tar --transform "s,^,$$d/," -czf engine1999_$$v.orig.tar.gz -T $$list && rm -f $$list && echo "made engine1999_$$v.orig.tar.gz"

# make clean: everything the build made (objects, programs, the library, the kit/ folder); keeps version.h (it cannot be made without git)
clean:
	rm -f *.o $(PROGS) engine.lib all gmon.out
	rm -rf kit

# make distclean: clean, and the settings of ./configure
distclean: clean
	rm -f config.mk
