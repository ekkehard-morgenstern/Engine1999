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

    ADDITIONAL PERMISSION (GPLv3 section 7): the Engine1999 Runtime Exception
        applies. Programs written in the built-in programming language, and
        executables made from them with BUILD, do not fall under this license
        as the exception describes (see the file LICENSE-EXCEPTION).

    CONTACT INFO:
        E-Mail: ekkehard@ekkehardmorgenstern.de
        Mail: Ekkehard Morgenstern, Mozartstr. 1, D-76744 Woerth am Rhein,
              Germany, Europe
*/

#include <stdio.h>

/*
The stand-in for engine1999.o in buildtest1: a main() that writes the embedded program (the file that BUILD binds into an executable,
see basbuild.h) to the standard output, as it is, and ends with the exit status 7.
*/

extern const unsigned char embedded_program_start[] __asm__( "_binary_program_e99_start" );
extern const unsigned char embedded_program_end[] __asm__( "_binary_program_e99_end" );

int main( int argc, char** argv ) {
    (void) argc; (void) argv;
    fwrite( embedded_program_start, 1U, (size_t)( embedded_program_end - embedded_program_start ), stdout );
    return 7;
}
