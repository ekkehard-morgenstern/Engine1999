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

CCOMP=gcc

ifdef DEBUG
DBGFLG=-g
else
DBGFLG=-O3
endif

# PROFILE=1 adds -p (every run writes gmon.out); off by default.
ifdef PROFILE
PRFFLG=-p
endif

CFLAGS=-Wall -Werror $(PRFFLG) $(DBGFLG) -march=native -mtune=native

CC=$(CCOMP) -c $(CFLAGS)
CL=$(CCOMP) $(CFLAGS)

.c.o:
	$(CC) -o $@ $<

all: sdltest1 sdltest2 engine1999 basictest1 testcomp1 basictest2 vmtest1 repltest1 ansitest1 txttest1 asmtest1 synthtest1 iotest1 gfxtest1 buildtest1 engine1999c contest1
	echo ok >all

.PHONY: grammar grammar-check kit install uninstall man-check

# the quick reference of the language (docs/grammar.md and docs/grammar.html) from bascomp.ebnf; needs perl
grammar:
	perl grammar.pl

# fails if a rule is used but not defined, or if the quick reference is out of date
grammar-check:
	perl grammar.pl --check

BASEMOD=sdlevent.o sdllayer.o sdlmain.o sdlscreen.o textscreen.o tilescreen.o sprscreen.o 8x12font1.o sdlaudio.o synth.o mml.o sdlutil.o \
		basic.o baspgm.o baslin.o bastok.o bascomp.o nodetype.o codegen.o cgenctl.o cgendecl.o cgenprog.o vars.o runtime.o vm.o basfiles.o iodev.o gfxcore.o gfxops.o basbuild.o basrepl.o basdebug.o ansi.o fulledit.o vmasm.o monitor.o
BASEHDR=sdlevent.h sdllayer.h sdlmain.h sdlscreen.h sdltypes.h textscreen.h tilescreen.h sprscreen.h unxtypes.h sdlaudio.h synth.h mml.h sdlutil.h basfiles.h iodev.h gfxcore.h gfxops.h basbuild.h basic.h baspgm.h baslin.h bastok.h bascomp.h nodetype.h codegen.h instset.h vars.h runtime.h vm.h basrepl.h basdebug.h ansi.h fulledit.h vmasm.h monitor.h

LIBMOD=$(BASEMOD)
LIBHDR=$(BASEHDR)
LIBNAME=engine.lib

sdltest1: sdltest1.o $(LIBNAME)
	$(CL) -o sdltest1 sdltest1.o $(LIBNAME) -lSDL2 -lrt -lm

sdltest1.o:	sdltest1.c $(LIBHDR)

sdltest2: sdltest2.o $(LIBNAME)
	$(CL) -o sdltest2 sdltest2.o $(LIBNAME) -lSDL2 -lrt -lm

sdltest2.o:	sdltest2.c $(LIBHDR)

engine1999: engine1999.o $(LIBNAME)
	$(CL) -o engine1999 engine1999.o $(LIBNAME) -lSDL2 -lrt -lm

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

txttest1: txttest1.o $(LIBNAME)
	$(CL) -o txttest1 txttest1.o $(LIBNAME) -lm

txttest1.o: txttest1.c $(LIBHDR)

iotest1: iotest1.o $(LIBNAME)
	$(CL) -o iotest1 iotest1.o $(LIBNAME) -lm

iotest1.o: iotest1.c $(LIBHDR)

gfxtest1: gfxtest1.o $(LIBNAME)
	$(CL) -o gfxtest1 gfxtest1.o $(LIBNAME) -lm

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
BINDIR=$(PREFIX)/bin
LIBDIR=$(PREFIX)/lib/engine1999
SHAREDIR=$(PREFIX)/share/engine1999
DOCDIR=$(PREFIX)/share/doc/engine1999
MANDIR=$(PREFIX)/share/man
KITFLAGS=-Wall -Werror -O2
KITOBJ=$(addprefix kit/,$(BASEMOD))

kit: kit/engine1999 kit/engine1999c kit/engine1999.o kit/engine1999c.o kit/engine.lib

$(KITOBJ) kit/engine1999.o kit/engine1999c.o: kit/%.o: %.c $(BASEHDR)
	@mkdir -p kit
	$(CCOMP) -c $(KITFLAGS) -o $@ $<

kit/engine1999.o kit/engine1999c.o: version.h

kit/engine.lib: $(KITOBJ)
	-rm -f kit/engine.lib
	ar q kit/engine.lib $(KITOBJ)

kit/engine1999: kit/engine1999.o kit/engine.lib
	$(CCOMP) $(KITFLAGS) -o $@ kit/engine1999.o kit/engine.lib -lSDL2 -lrt -lm

kit/engine1999c: kit/engine1999c.o kit/engine.lib
	$(CCOMP) $(KITFLAGS) -o $@ kit/engine1999c.o kit/engine.lib -lrt -lm

install: kit version.h
	install -d $(DESTDIR)$(BINDIR) $(DESTDIR)$(LIBDIR) $(DESTDIR)$(SHAREDIR)/demos $(DESTDIR)$(DOCDIR)/docs/images \
		$(DESTDIR)$(MANDIR)/man1 $(DESTDIR)$(MANDIR)/man7
	install -m 755 kit/engine1999 kit/engine1999c $(DESTDIR)$(BINDIR)
	install -m 644 kit/engine1999.o kit/engine1999c.o kit/engine.lib $(DESTDIR)$(LIBDIR)
	install -m 644 demos/*.bas demos/README.md $(DESTDIR)$(SHAREDIR)/demos
	install -m 644 README.md LICENSE LICENSE-EXCEPTION $(DESTDIR)$(DOCDIR)
	install -m 644 docs/*.md docs/grammar.html $(DESTDIR)$(DOCDIR)/docs
	install -m 644 docs/images/*.png $(DESTDIR)$(DOCDIR)/docs/images
	@v=$$(sed -n 's/.*ENGINE_VERSION *"\(.*\)".*/\1/p' version.h); \
	for p in man/*.1; do sed "s/@VERSION@/$$v/" $$p > $(DESTDIR)$(MANDIR)/man1/$$(basename $$p); chmod 644 $(DESTDIR)$(MANDIR)/man1/$$(basename $$p); done; \
	for p in man/*.7; do sed "s/@VERSION@/$$v/" $$p > $(DESTDIR)$(MANDIR)/man7/$$(basename $$p); chmod 644 $(DESTDIR)$(MANDIR)/man7/$$(basename $$p); done
	@echo "installed in $(DESTDIR)$(PREFIX): engine1999, engine1999c, the BUILD kit in $(DESTDIR)$(LIBDIR), man pages (man engine1999)"

uninstall:
	rm -f $(DESTDIR)$(BINDIR)/engine1999 $(DESTDIR)$(BINDIR)/engine1999c
	rm -rf $(DESTDIR)$(LIBDIR) $(DESTDIR)$(SHAREDIR) $(DESTDIR)$(DOCDIR)
	for p in man/*.1; do rm -f $(DESTDIR)$(MANDIR)/man1/$$(basename $$p); done
	for p in man/*.7; do rm -f $(DESTDIR)$(MANDIR)/man7/$$(basename $$p); done

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

mml.o: mml.c $(BASEHDR)

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

iodev.o: iodev.c $(BASEHDR)

gfxcore.o: gfxcore.c $(BASEHDR)

gfxops.o: gfxops.c $(BASEHDR)

basrepl.o: basrepl.c $(BASEHDR)

basdebug.o: basdebug.c $(BASEHDR)

ansi.o: ansi.c $(BASEHDR)

fulledit.o: fulledit.c $(BASEHDR)

vmasm.o: vmasm.c $(BASEHDR)

monitor.o: monitor.c $(BASEHDR)
