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

#ifndef INSTSET_H
#define INSTSET_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
The runtime system has two stacks:

    - a data stack for holding parameters and return values
      every item is a 64 bit floating-point number

    - a code stack for holding return addresses
      every item is a 32 bit unsigned address offset

An instruction removes its parameters from the stack.
We're using FORTH notation here to indicate stack usage.
For instance,
    ( n1 n2 -- n )  means "parameters are n1 and n2, in that order" and
                        "n" is the return value.
    R( a -- )       means an address offset is pulled from the return stack.

    n -- a 64-bit double (floating-point) value (data stack)
    a -- a 32-bit unsigned address offset (return stack only)
    i -- a 16-bit signed integer (not on stack)
    u -- a 16-bit unsigned integer (not on stack)
    s -- a 64-bit string pointer (data stack)

ATTN: String pointers point to heap memory and must be freed after use.
      Instructions with string arguments free their arguments automatically.

Instructions are encoded as follows:

    <ins> [ <exp> ] [ <hi> <lo> ]           or, with the W flag:      <ins> [ <exp> ] [ <b3> <b2> <b1> <b0> ]

First, an instruction byte.
Followed by an optional 8-bit instruction extension.
Followed by an optional parameter field in network byte order (big endian): 16 bits, or 32 bits if the W flag
(wide) is set. The generator uses the short form where a value fits (variable numbers, small data offsets, small
immediates) and the wide form for everything that can be above 65535: code addresses (always, because they are
patched after the instruction was generated), data offsets, line numbers and large immediates.

If the instruction extension is requested, an additional byte follows, expanding
the instruction field from 4 bit to 12 bits, with the 4 bits from the instruction byte
being the most significant, and the 8 bits from the extension byte being the least
significant bits.

if the parameter is given, it specifies an offset either into the data segment, or
the code segment, depending on a flag in the instruction code.

the instruction code is organized as follows:

    +---+---+---+---++---+---+---+---+
    | P | C | E | W || I | I | I | I |
    +---+---+---+---++---+---+---+---+

    P - parameter flag (parameter field is present)
    C - code flag (code, not data offset)
    E - extended instruction
    W - wide parameter (the parameter field has 32 bits; only together with P)
    I - an instruction field

The 16 basic instructions are as follows:

    0000 - BRK          a breakpoint (bits C and P are ignored, and should be 0)
    0001 - NOP          no operation (bits C and P are ignored, and should be 0)

    0010 - PHPA         push parameter address onto stack
                        the C bit decides which stack (code or data)
                        C=0 data stack, item is converted to floating-point
                        C=1 return stack, item is stored as an integer
                        the P bit must be set (and a parameter field supplied)

    0011 - PHIM         push 17 bit immediate value on stack (from parameter field)
                        the C bit becomes the uppermost (17th) bit.
                        With the W flag the 32-bit parameter is the (signed) value and the C bit is ignored.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is sign-extended to 32 bits, and then
                        converted to a number before being pushed onto the stack.
                        this is to sacrifice code space over data space for small
                        integral numbers.

    0100 - BRIA         branch to immediate 17-bit address
                        the C bit becomes the uppermost (17th) bit.
                        With the W flag the 32-bit parameter is the address and the C bit is ignored (the code
                        generator always uses the wide form).
                        the P bit must be set (and a parameter field supplied)
                        first, the value is zero-extended to 32 bits, and then
                        used as an index into code memory (when in range).
                        When out of range, a runtime exception occurs.

    0101 - BRIR         branch immediate relative, using 17-bit offset
                        the C bit becomes the uppermost (17th) bit. With the W flag: a signed 32-bit offset.
                        the P bit must be set (and a parameter field supplied)
                        first, the value is sign-extended to 32 bits, and then
                        is added to the address of the following instruction.
                        if the result address is in range, a branch takes place.
                        When out of range, a runtime exception occurs.

    0110 - JPCC         quick conditional jump ( n -- ) R( a -- )
                        pulls one data item (numeric) and one return address item.
                        if the data item cast to an integer is nonzero, execution
                        continues at the specified code address.
                        the C bit must be set to 1, the P bit must be 0.
                        When out of range, a runtime exception occurs.

    0111 - JUMP         quick jump R( a -- )
                        pulls one return address and jumps to it.
                        the C bit must be set to 1, the P bit must be 0.
                        When out of range, a runtime exception occurs.

    1000 - DROP         drop stack item(s) ( n -- ) or R( a -- )
                        If C is 0, operates on the data stack.   (item size: 8 bytes)
                        If C is 1, operates on the return stack. (item size: 2 bytes)
                        if a parameter field is also given, it specifies the number of
                        items to drop.

    1001 - LINE         set line number immediate
                        sets the current line number (must be the first instruction of a line)
                        C field must be 0, P must be set and a line number specified as parameter.

    1010 - LAN          load as number ( n -- n )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack
                        note that the address must be given as a floating-point number

    1011 - LIAN         load integer as number ( n -- n )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack (after converting it to a number - signed)
                        note that the address must be given as a floating-point number

    1100 - LUAN         load unsigned as number ( n -- n )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack (after converting it to a number - unsigned)
                        note that the address must be given as a floating-point number

    1101 - LAS          load as string ( n -- s )
                        loads value pointed to by code or data address on stack (flag C)
                        onto the stack (after converting it to a string pointer)
                        note that the address must be given as a floating-point number

    1110 - FRES         free string ( s -- )
                        frees string pointer from data stack (flag C must be 0)

    1111 - reserved

The 4096 extended instructions are as follows:

    0000 0000xxxx       same as basic instructions
    0000 00010000       NEG - numerical negation                    ( n -- n )      C=0 P=0
    0000 00010001       NOT - bitwise NOT                           ( n -- n )      C=0 P=0
    0000 00010010       LSH - left shift                            ( n1 n2 -- n )  C=0 P=0
    0000 00010011       RSH - right shift                           ( n1 n2 -- n )  C=0 P=0
    0000 00010100       ADD - numerical addition                    ( n1 n2 -- n )  C=0 P=0
    0000 00010101       SUB - numerical subtraction                 ( n1 n2 -- n )  C=0 P=0
    0000 00010110       MUL - numerical multiplication              ( n1 n2 -- n )  C=0 P=0
    0000 00010111       DIV - numerical division                    ( n1 n2 -- n )  C=0 P=0
    0000 00011000       AND - bitwise AND                           ( n1 n2 -- n )  C=0 P=0
    0000 00011001       NAND - bitwise NAND                         ( n1 n2 -- n )  C=0 P=0
    0000 00011010       OR  - bitwise OR                            ( n1 n2 -- n )  C=0 P=0
    0000 00011011       NOR - bitwise NOR                           ( n1 n2 -- n )  C=0 P=0
    0000 00011100       XOR - bitwise XOR                           ( n1 n2 -- n )  C=0 P=0
    0000 00011101       XNOR - bitwise XNOR                         ( n1 n2 -- n )  C=0 P=0
    0000 00011110       POW - power                                 ( n1 n2 -- n )  C=0 P=0
    0000 00011111       CON - concatenate strings                   ( s1 s2 -- s )  C=0 P=0
    0000 00100000       CNEQ - compare numerical equal              ( n1 n2 -- n )  C=0 P=0
    0000 00100001       CNNE - compare numerical not equal          ( n1 n2 -- n )  C=0 P=0
    0000 00100010       CNGE - compare numerical greater or equal   ( n1 n2 -- n )  C=0 P=0
    0000 00100011       CNLE - compare numerical less or equal      ( n1 n2 -- n )  C=0 P=0
    0000 00100100       CNGT - compare numerical greater than       ( n1 n2 -- n )  C=0 P=0
    0000 00100101       CNLT - compare numerical less than          ( n1 n2 -- n )  C=0 P=0
    0000 00100110       CSEQ - compare string equal                 ( s1 s2 -- n )  C=0 P=0
    0000 00100111       CSNE - compare string not equal             ( s1 s2 -- n )  C=0 P=0
    0000 00101000       CSGE - compare string greater or equal      ( s1 s2 -- n )  C=0 P=0
    0000 00101001       CSLE - compare string less or equal         ( s1 s2 -- n )  C=0 P=0
    0000 00101010       CSGT - compare string greater than          ( s1 s2 -- n )  C=0 P=0
    0000 00101011       CSLT - compare string less than             ( s1 s2 -- n )  C=0 P=0
        .
        .
        .

    0001 00000000       RRNV - read regular numeric variable (param=varoffs) ( -- n ) C=0 P=1
    0001 00000001       RRIV - read regular integer variable (param=varoffs) ( -- n ) C=0 P=1
    0001 00000010       RRSV - read regular string  variable (param=varoffs) ( -- s ) C=0 P=1
    0001 00000011       RRLV - read regular label   variable (param=varoffs) R( -- a ) C=1 P=1
    0001 00000100       RNAE - read numeric array element (param=varoffs) ( inx ... ndim -- n ) C=0 P=1
    0001 00000101       RIAE - read integer array element (param=varoffs) ( inx ... ndim -- n ) C=0 P=1
    0001 00000110       RSAE - read string  array element (param=varoffs) ( inx ... ndim -- s ) C=0 P=1
    0001 00000111       (reserved)

    0001 00001000       WRNV - write regular numeric variable (param=varoffs) ( n -- ) C=0 P=1
    0001 00001001       WRIV - write regular integer variable (param=varoffs) ( n -- ) C=0 P=1
    0001 00001010       WRSV - write regular string  variable (param=varoffs) ( s -- ) C=0 P=1
    0001 00001011       WRLV - write regular label   variable (param=varoffs) R( a -- ) C=1 P=1
    0001 00001100       WNAE - write numeric array element (param=varoffs) ( n inx ... ndim -- ) C=0 P=1
    0001 00001101       WIAE - write integer array element (param=varoffs) ( n inx ... ndim -- ) C=0 P=1
    0001 00001110       WSAE - write string  array element (param=varoffs) ( s inx ... ndim -- ) C=0 P=1
    0001 00001111       (reserved)

    0001 00010000       SHEX - shell execute ( s -- s ) C=0 P=0
    0001 00010001       (reserved)
    0001 00010010       CALF - call function (param=varoffs) ( ... args n -- ) R( -- retoffs context... ) C=0 P=1
                        Operation:
                            - the return address is put on the return stack
                            - the arguments are pulled from the stack and written into a temporary space
                            - the context for the call is created and pushed onto the return stack (pointer):
                                - contains the varoffs and content of each regular parameter variable before the call
                                  (saving array state isn't necessary)
                            - each regular parameter variable is set to one of the values from the temporary space
                            - code execution continues with the code of the function
    0001 00010011       RETF - return from function call ( result -- result ) R( retoffs context... -- ) C=0 P=0
                            - the context is pulled from the return stack, and variable contents are restored
                            - execution resumes with the instruction after the CALF instruction.
                            - the data stack is not affected (a result placed there will still exist afterwards)
    0001 00010100       GADC - get array    dimension count (param=varoffs) ( -- n ) C=0 P=1
    0001 00010101       GFAC - get function argument  count (param=varoffs) ( -- n ) C=0 P=1
    0001 00010110       ERAS - erase array (param=varoffs)   ( -- ) C=0 P=1
                            resets all elements of the array to their initial state (0 or the empty string)
    0001 00010111       RDNM - read next DATA item as a number ( -- n ) C=0 P=0
    0001 00011110       RDST - read next DATA item as a string ( -- s ) C=0 P=0
    0001 00011111       RSTR - restore the DATA pointer (param=data offset) C=0 P=0/1
                            P=0: the pointer goes back to the first item; P=1: to the item at the given data
                            offset (0xFFFFFFFF = behind the last item). With a parameter the instruction is wide
                            (the offset is patched in when all DATA items are known).
                            DATA items are records in the data segment, chained in program order:
                            <type.8> <next.32> <payload>, type 0 = number (8 bytes, big endian IEEE double),
                            type 1 = string (NUL-terminated text); <next> is the data offset of the next item or
                            0xFFFFFFFF. The first item is found through codegen_t.datafirst (0xFFFFFFFF = no DATA).
                            Reading behind the last item, or an item of the wrong type, is a run time error.
    0001 00010110       (reserved)
    0001 00010111       (reserved)
    0001 00011000       TI   - get system   timer ( -- n ) C=0 P=0
    0001 00011001       INKY - get keyboard input ( -- s ) C=0 P=0
    0001 00011010       ASC  - ASCII code of first character        ( s -- n ) C=0 P=0
    0001 00011011       VAL  - numeric value of string              ( s -- n ) C=0 P=0
    0001 00011100       FRE  - return amount of free memory         ( n -- n ) C=0 P=0
    0001 00011101       LEN  - return length of string              ( s -- n ) C=0 P=0
    0001 00011110       (reserved)
    0001 00011111       (reserved)
    0001 00100000       BINS - convert number to binary      string ( n n -- s ) C=0 P=0
    0001 00100001       QUAS - convert number to quaternary  string ( n n -- s ) C=0 P=0
    0001 00100010       OCTS - convert number to octal       string ( n n -- s ) C=0 P=0
    0001 00100011       HEXS - convert number to hexadecimal string ( n n -- s ) C=0 P=0
    0001 00100100       DECS - convert number to decimal     string ( n n -- s ) C=0 P=0
    0001 00100101       STRS - convert number to             string ( n   -- s ) C=0 P=0
    0001 00100110       CHRS - convert character code to string ( n -- s ) C=0 P=0
                            n must be 1..255 (a string can't hold a NUL character), else a run time error
    0001 00100111       (reserved)
    0001 00101000       LFTS - get left part of string ( s n -- s ) C=0 P=0
    0001 00101001       RGTS - get right part of string ( s n -- s ) C=0 P=0
    0001 00101010       MIDS - get mid part of string ( s n n -- s ) C=0 P=0
    0001 00101011       (reserved)
    0001 00101100       SLFT - set left part of string variable   (param=varoffs) ( s n inx ... ndim -- ) C=0 P=1
    0001 00101101       SRGT - set right part of string variable  (param=varoffs) ( s n inx ... ndim -- ) C=0 P=1
    0001 00101110       SMID - set mid part of string variable    (param=varoffs) ( s start len inx ... ndim -- ) C=0 P=1
                        The variable is a string variable (ndim = 0) or an element of a string array (the
                        subscripts, as for WSAE). The string s replaces characters of the variable's value:
                            SLFT: the first  min( n, LEN(s), LEN(var) ) characters
                            SRGT: the last   min( n, LEN(s), LEN(var) ) characters
                            SMID: min( len, LEN(s), LEN(var) - start + 1 ) characters beginning at <start> (counts
                                  from 1); a negative <len> stands for the rest of the string
                        The characters come from the beginning of s, the length of the variable doesn't change.
                        n < 0, start < 1 or start > LEN(var) are errors.
    0001 00101111       (reserved)
    0001 00110000       SAVE - save program ( s s -- ) C=0 P=0
    0001 00110001       PCHN - print channel ( n -- ) C=0 P=0
    0001 00110010       PNUM - print number (param=separator) ( n -- ) C=0 P=0/1
    0001 00110011       PSTR - print string (param=separator) ( n -- ) C=0 P=0/1
    0001 00110100       INPL - input line (param=data offset of the prompt text) ( -- ) C=0 P=1
                            Prints the prompt (NUL-terminated text in the data segment, to the current channel, see
                            PCHN) and reads a line from the current channel into the VM's input buffer. The
                            instruction is the restart point of the INPUT statement (see INPN).
    0001 00110101       INPN - take the next input field as a number ( -- n ) C=0 P=0
    0001 00110110       INPS - take the next input field as a string ( -- s ) C=0 P=0
                            Fields of the input line are separated by commas; a field starting with a double quote
                            extends to the closing quote (the quotes are removed). A numeric field is a decimal
                            number with optional sign, fraction and exponent, surrounded by blanks; an empty field
                            is 0. A string field that isn't quoted is trimmed of blanks at both ends.
                            If the field is missing, or isn't a number for INPN, the VM prints "?Redo from start"
                            and execution continues at the INPL instruction of the statement (fields already
                            stored stay stored).
    0001 00110111       INPE - end of INPUT ( -- ) C=0 P=0
                            If the line has more fields than the statement has variables, prints "?Extra ignored".
                            Releases the input buffer.
    0001 00111000       GETK - wait for a key and push it as a one character string ( -- s ) C=0 P=0
                            (GET stores it directly in a string variable, or converts it with ASC for a numeric one)
    0001 00111001       LOAD - load program ( s -- ) C=0 P=0
                            Asks the host to replace the program by the one in file <s> and to run it from the
                            beginning (all variables cleared). The VM stores the request (vm_t.request) and stops;
                            the host that called vm_run() carries it out. An empty name is an error.
    0001 00111010       RUNP - run program ( s n -- ) C=0 P=0
                            Asks the host to run the stored program from a line number (n >= 0) or a label (name s,
                            if not empty) after clearing all variables; ( "" -1 ) = from the beginning. Same
                            mechanism as LOAD.
    0001 00111011       LIST - list program ( s1 n1 s2 n2 -- ) C=0 P=0
                            Lists the stored program through the host (list callback) to the current output channel.
                            (s1,n1) is the first line, (s2,n2) the last one: a label name (empty = none) or a line
                            number (-1 = none). A side that is not given is unbounded; a label stands for the line it
                            is defined in. Both sides given = an inclusive range.
    0001 00111100       NEW  - new program ( -- ) C=0 P=0
                            Asks the host to delete the program and the variables; the VM stores the request and
                            stops (like LOAD), the program is not continued.
    0001 00111110       DELE - delete program lines ( s1 n1 s2 n2 -- ) C=0 P=0
                            Deletes the lines in the range (same operands as LIST) through the host (delete callback),
                            then ends the run: the stored program no longer matches the code that is running.
    0001 00111111       RENM - renumber program lines ( s1 n1 s2 n2 step -- ) C=0 P=0
                            Renumbers the lines of the range (operands as for LIST) with the new numbers starting at the
                            first line of the range (10 if the start is not given), through the host (renumber
                            callback); then ends the run like DELE. step is a number >= 1.
    0001 00111101       CLR  - clear variables ( -- ) C=0 P=0
                            Numeric variables become 0, strings "", arrays are emptied (they keep their dimensions);
                            labels, functions and the compiler's hidden variables are kept.
    0001 01000000       EDIT - edit a program line ( s n -- ) C=0 P=0
                            Asks the host to let the user edit the line given by a line number (n >= 0) or a label
                            (name s, if not empty); ( "" -1 ) = no target. Stores a request like RUNP and ends the run.
    0001 01000001       MON  - open the machine monitor ( -- ) C=0 P=0
                            Stores a request for the host (like NEW) and ends the run.
    0001 01000010       SND  - sound statement ( args -- ) C=0 P=1; the parameter is the kind (SNDK_* in nodetype.h):
                            1 SOUND ( ch freq beats vol endfreq -- )   a tone: channel 1..8, 20..20000 Hz (0 = a rest),
                              beats >= 0 (the channel's next event follows after that), volume 0..1; endfreq != 0 (20..20000,
                              needs beats > 0): the pitch glides to it during the tone
                            2 PLAY ( s1 .. sn n -- )           n music strings (1..8): string i plays on channel i, all
                              voices start together (the strings are parsed first: a wrong one queues nothing, see mml.h)
                            3 WAVE ( ch wave -- )              wave type 0..3 (sine, square, triangle, noise)
                            4 TEMPO ( bpm -- )                 beats per minute 1..1000 (the unit of the beats of SOUND)
                            5 OFF ( ch -- )                    silence the channel now and forget what is queued; 0 = all
                            6 SYNC ( ch1 .. chn n -- )         line up the channels' time (so that they start together)
                            7 PITCH ( ch freq -- )             at once: the pitch of the channel, 20..20000 Hz
                            8 VOLUME ( ch vol -- )             at once: the volume of the note, 0..1
                            9 ENVELOPE ( ch a d s r -- )       attack, decay (s), sustain level 0..1, release (s), each time
                              0..60; a < 0 = back to the default
                            10 VIBRATO ( ch rate depth -- )    rate 0..100 Hz, depth 0..24 semitones (0 = off)
                            11 TREMOLO ( ch rate depth -- )    rate 0..100 Hz, depth 0..1 (0 = off)
                            12 SFXDEF ( n s -- )               store the music string s as effect n (1..64)
                            13 SFXPLAY ( n ch -- )             play effect n on channel ch (0 = a free one)
                            Everything but 7 and 8 is queued at the channel's time line; all return at once. Without sound
                            callbacks the arguments are only checked. A bad argument is "Illegal argument".
    0001 01000011       PLNG - is the channel busy? ( ch -- n ) C=0 P=0
                            -1 while channel 1..8 sounds or has events queued or a sequence still running, else 0 (also
                            without sound callbacks). Other channels are "Illegal argument".
    0001 01000100       WAIT - sleep ( n -- ) C=0 P=0
                            Sleeps for n/60 seconds (n >= 0, at most 216000 = one hour; the unit of TI) without using the
                            processor. The interrupt callback is asked every 10 ms, so Break ends the wait ("Break").
    0001 01000101       MRG  - merge a program file ( s -- ) C=0 P=1
                            Merges the program file <s> into the stored program through the host (merge callback); the
                            parameter is 1 for APPEND (the file's lines are numbered behind the last stored line). Then ends the
                            run: the stored program no longer matches the code that is running. An empty name is an error.
    0001 01000110       CHN  - chain to a program ( s1 s2 n -- ) C=0 P=1
                            Asks the host to load the file <s1> (parameter 1: merge it into the stored program) and to run the
                            result from the label s2 ("" = none) or the line n (-1 = none), or from the beginning; the values of
                            the variables that both programs have are kept. Request VMREQ_CHAIN, like LOAD.
    0001 01000111       FILS - list files ( s -- ) C=0 P=0
                            Lists the files named by <s> ("" = the current directory) through the host (files callback) to
                            the current output channel: size, time and name, oldest first. The run goes on.

*/

#define INS_NODATA      UINT32_C(0XFFFFFFFF)
#define INSF_P          UINT8_C(0X80)
#define INSF_C          UINT8_C(0X40)
#define MKINS_C(x)      ( ((x) & UINT8_C(1)) << UINT8_C(6) )
#define INSF_E          UINT8_C(0X20)
#define INSF_W          UINT8_C(0X10)
#define INSM_I          UINT8_C(0X0F)
#define MKINS_I(x)      ((x) & INSM_I)
#define INS_BRK         MKINS_I(0)
#define INS_NOP         MKINS_I(1)
#define INS_PHPA        MKINS_I(2)
#define INS_PHPA_C      ( INS_PHPA | INSF_C )
#define INS_PHPA_D      INS_PHPA
#define INS_PHIM        MKINS_I(3)
#define INS_BRIA        MKINS_I(4)
#define INS_BRIR        MKINS_I(5)
#define INS_JPCC        MKINS_I(6)
#define INS_JUMP        MKINS_I(7)
#define INS_DROP        MKINS_I(8)
#define INS_LINE        MKINS_I(9)
#define INS_LAN         MKINS_I(10)
#define INS_LIAN        MKINS_I(11)
#define INS_LUAN        MKINS_I(12)
#define INS_LAS         MKINS_I(13)
#define INS_FRES        MKINS_I(14)

#define INS_NEG         UINT16_C(0X010)
#define INS_NOT         UINT16_C(0X011)
#define INS_LSH         UINT16_C(0X012)
#define INS_RSH         UINT16_C(0X013)
#define INS_ADD         UINT16_C(0X014)
#define INS_SUB         UINT16_C(0X015)
#define INS_MUL         UINT16_C(0X016)
#define INS_DIV         UINT16_C(0X017)
#define INS_AND         UINT16_C(0X018)
#define INS_NAND        UINT16_C(0X019)
#define INS_OR          UINT16_C(0X01A)
#define INS_NOR         UINT16_C(0X01B)
#define INS_XOR         UINT16_C(0X01C)
#define INS_XNOR        UINT16_C(0X01D)
#define INS_POW         UINT16_C(0X01E)
#define INS_CON         UINT16_C(0X01F)
#define INS_CNEQ        UINT16_C(0X020)
#define INS_CNNE        UINT16_C(0X021)
#define INS_CNGE        UINT16_C(0X022)
#define INS_CNLE        UINT16_C(0X023)
#define INS_CNGT        UINT16_C(0X024)
#define INS_CNLT        UINT16_C(0X025)
#define INS_CSEQ        UINT16_C(0X026)
#define INS_CSNE        UINT16_C(0X027)
#define INS_CSGE        UINT16_C(0X028)
#define INS_CSLE        UINT16_C(0X029)
#define INS_CSGT        UINT16_C(0X02A)
#define INS_CSLT        UINT16_C(0X02B)
#define INS_RRNV        UINT16_C(0X100)
#define INS_RRIV        UINT16_C(0X101)
#define INS_RRSV        UINT16_C(0X102)
#define INS_RRLV        UINT16_C(0X103)
#define INS_RNAE        UINT16_C(0X104)
#define INS_RIAE        UINT16_C(0X105)
#define INS_RSAE        UINT16_C(0X106)
#define INS_WRNV        UINT16_C(0X108)
#define INS_WRIV        UINT16_C(0X109)
#define INS_WRSV        UINT16_C(0X10A)
#define INS_WRLV        UINT16_C(0X10B)
#define INS_WNAE        UINT16_C(0X10C)
#define INS_WIAE        UINT16_C(0X10D)
#define INS_WSAE        UINT16_C(0X10E)
#define INS_SHEX        UINT16_C(0X110)
#define INS_CALF        UINT16_C(0X112)
#define INS_RETF        UINT16_C(0X113)
#define INS_GADC        UINT16_C(0X114)
#define INS_GFAC        UINT16_C(0X115)
#define INS_ERAS        UINT16_C(0X116)
#define INS_RDNM        UINT16_C(0X117)
#define INS_TI          UINT16_C(0X118)
#define INS_INKY        UINT16_C(0X119)
#define INS_ASC         UINT16_C(0X11A)
#define INS_VAL         UINT16_C(0X11B)
#define INS_FRE         UINT16_C(0X11C)
#define INS_LEN         UINT16_C(0X11D)
#define INS_RDST        UINT16_C(0X11E)
#define INS_RSTR        UINT16_C(0X11F)
#define INS_BINS        UINT16_C(0X120)
#define INS_QUAS        UINT16_C(0X121)
#define INS_OCTS        UINT16_C(0X122)
#define INS_HEXS        UINT16_C(0X123)
#define INS_DECS        UINT16_C(0X124)
#define INS_STRS        UINT16_C(0X125)
#define INS_CHRS        UINT16_C(0X126)
#define INS_LFTS        UINT16_C(0X128)
#define INS_RGTS        UINT16_C(0X129)
#define INS_MIDS        UINT16_C(0X12A)
#define INS_SLFT        UINT16_C(0X12C)
#define INS_SRGT        UINT16_C(0X12D)
#define INS_SMID        UINT16_C(0X12E)
#define INS_SAVE        UINT16_C(0X130)
#define INS_PCHN        UINT16_C(0X131)
#define INS_PNUM        UINT16_C(0X132)
#define INS_PSTR        UINT16_C(0X133)
#define INS_INPL        UINT16_C(0X134)
#define INS_INPN        UINT16_C(0X135)
#define INS_INPS        UINT16_C(0X136)
#define INS_INPE        UINT16_C(0X137)
#define INS_GETK        UINT16_C(0X138)
#define INS_LOAD        UINT16_C(0X139)
#define INS_RUNP        UINT16_C(0X13A)
#define INS_LIST        UINT16_C(0X13B)
#define INS_NEW         UINT16_C(0X13C)
#define INS_CLR         UINT16_C(0X13D)
#define INS_DELE        UINT16_C(0X13E)
#define INS_RENM        UINT16_C(0X13F)
#define INS_EDIT        UINT16_C(0X140)
#define INS_MON         UINT16_C(0X141)
#define INS_SND         UINT16_C(0X142)
#define INS_PLNG        UINT16_C(0X143)
#define INS_WAIT        UINT16_C(0X144)
#define INS_MRG         UINT16_C(0X145)
#define INS_CHN         UINT16_C(0X146)
#define INS_FILS        UINT16_C(0X147)

#endif
