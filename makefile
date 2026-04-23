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

all: sdltest1 engine1999 basictest1
	echo ok >all

BASEMOD=sdlevent.o sdllayer.o sdlmain.o sdlscreen.o textscreen.o tilescreen.o sprscreen.o 8x12font1.o sdlaudio.o sdlutil.o basic.o
BASEHDR=sdlevent.h sdllayer.h sdlmain.h sdlscreen.h sdltypes.h textscreen.h tilescreen.h sprscreen.h unxtypes.h sdlaudio.h sdlutil.h basic.h

LIBMOD=$(BASEMOD)
LIBHDR=$(BASEHDR)
LIBNAME=engine.lib

sdltest1: sdltest1.o $(LIBNAME)
	$(CL) -o sdltest1 sdltest1.o $(LIBNAME) -lSDL2 -lrt -lm

sdltest1.o:	sdltest1.c $(LIBHDR)

engine1999: engine1999.o $(LIBNAME)
	$(CL) -o engine1999 engine1999.o $(LIBNAME) -lSDL2 -lrt -lm

engine1999.o: engine1999.c $(LIBHDR)

basictest1: basictest1.o $(LIBNAME)
	$(CL) -o basictest1 basictest1.o $(LIBNAME) -lm

basictest1.o: basictest1.c $(LIBHDR)

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

sdlutil.o: sdlutil.c $(BASEHDR)

basic.o: basic.c $(BASEHDR)
