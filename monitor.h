#pragma once

/*
    Engine1999 - A 2D games engine written in C
    Copyright (C) 2026  Ekkehard Morgenstern

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.

    NOTE: Programs created with a built-in programming language (if any),
          do not fall under this license.

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/
#ifndef MONITOR_H
#define MONITOR_H       1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASREPL_H
#include "basrepl.h"
#endif

/*
Machine monitor (MONITOR statement), host side: it needs the SDL screen and the REPL. It runs on text screen 2 (BASIC
keeps screen 0, the full-screen editor screen 1), with a title bar, a scrolling output area where commands are typed
and answered (syntax colored), and a help bar. The screen keeps its contents between calls.

It looks at the code and data segments of the REPL's code generator and at the VM's stacks, and uses the instruction set
of the VM (instset.h, vmasm.h) for assembling and disassembling. When it opens and nothing is compiled yet, the stored
program is compiled (not run).

Numbers in commands are hexadecimal: $1F, 0x1F or 1F; #31 is decimal. The assembler's operands are decimal unless
they have $ or 0x.

    ?              help
    r [region]     select the region to look at (code, data, dstack, rstack), or show all regions
    m [s [e]]      memory dump (code, data) or stack listing, from s (default: where the last dump ended) to e
    u [s [e]]      unassemble code (also d)
    a [s]          assemble: one instruction per line at s, an empty line ends; may overwrite code, or append at its end
    e s b...       enter bytes at s (code or data)
    f s e b        fill s..e with byte b (code or data)
    l n            find BASIC line n in the code (address); sets the default address
    c              compile the stored program again (discards patches)
    g [s]          run the code from s as it is now, or continue the suspended run; stops at breakpoints and BRK; ESC breaks
    s [s]          suspend the VM at s (default 0) without running: the start of a step-by-step run
    t [n]          run n instructions (default 1), showing each; a '>' marks the pc, a '*' a breakpoint
    b [a]          set a breakpoint at the instruction address a; without an address: list them
    k a | *        remove the breakpoint at a / all of them
    i              state of the VM: registers, top of the data stack, breakpoints
    q              leave the monitor

Writing to the code is at the user's own risk: a wrong instruction can make the VM fail. RUN compiles the program again, so
patches are only kept for g, s and t. Breakpoints are cleared by c (the addresses mean nothing in new code).
*/

// returns when the user leaves the monitor (or the screen is closed)
void monitor_run( repl_t* repl, void (*vblank_handler)( void* ), void* usrdata );

#endif
