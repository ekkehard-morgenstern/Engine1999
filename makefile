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
#   NOTE: Programs created with a built-in programming language (if any),
#         do not fall under this license.
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

CFLAGS=-Wall -Werror -p $(DBGFLG) -march=native -mtune=native

CC=$(CCOMP) -c $(CFLAGS)
CL=$(CCOMP) $(CFLAGS)

.c.o:
	$(CC) -o $@ $<

all: sdltest1 sdltest2 engine1999 basictest1 testcomp1 basictest2 vmtest1 repltest1 ansitest1 txttest1 asmtest1 synthtest1
	echo ok >all

BASEMOD=sdlevent.o sdllayer.o sdlmain.o sdlscreen.o textscreen.o tilescreen.o sprscreen.o 8x12font1.o sdlaudio.o synth.o mml.o sdlutil.o \
		basic.o baspgm.o baslin.o bastok.o bascomp.o nodetype.o codegen.o cgenctl.o cgendecl.o cgenprog.o vars.o runtime.o vm.o basfiles.o basrepl.o basdebug.o ansi.o fulledit.o vmasm.o monitor.o
BASEHDR=sdlevent.h sdllayer.h sdlmain.h sdlscreen.h sdltypes.h textscreen.h tilescreen.h sprscreen.h unxtypes.h sdlaudio.h synth.h mml.h sdlutil.h basfiles.h basic.h baspgm.h baslin.h bastok.h bascomp.h nodetype.h codegen.h instset.h vars.h runtime.h vm.h basrepl.h basdebug.h ansi.h fulledit.h vmasm.h monitor.h

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

engine1999.o: engine1999.c $(LIBHDR)

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

basrepl.o: basrepl.c $(BASEHDR)

basdebug.o: basdebug.c $(BASEHDR)

ansi.o: ansi.c $(BASEHDR)

fulledit.o: fulledit.c $(BASEHDR)

vmasm.o: vmasm.c $(BASEHDR)

monitor.o: monitor.c $(BASEHDR)
