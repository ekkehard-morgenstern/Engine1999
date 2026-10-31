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

#ifndef BASTOK_H
#define BASTOK_H    1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*

   The program store keeps only the tokenized form of each line. Before running a
   program, it is compiled (syntax tree, then stack-machine code), so the text is
   parsed only once.

   Rationale behind the encoding of various literals:
    - Every literal should have a length byte, so it is easy to read back in.
    - Numbers should be encoded as strings, so when you save a program, it is saved
      in an architecture independent format (i.e. regardless of the numeric precision
      of the host machine). Thus, a number conversion error is a runtime error
      (which might be reported before actually running the program, see above).
    - Numerals 0..9 when appearing alone are encoded as separate tokens to speed up
      reading of one-digit numbers.

*/

#define TOK_EOL         UINT8_C(0)          // end of line
#define TOK_IDENT       UINT8_C(1)          // identifier, <len> <chr> ...
#define TOK_BINLIT      UINT8_C(2)          // binary literal, <len> <digchr> ...
#define TOK_STRLIT      UINT8_C(3)          // string literal, <len> <chr> ...
#define TOK_QUALIT      UINT8_C(4)          // quaternary literal, <len> <digchr> ...
#define TOK_SHLLIT      UINT8_C(5)          // shell literal, <len> <chr> ...
#define TOK_QUOLIT      UINT8_C(6)          // quote literal, <len> <chr> ...
#define TOK_BRKLIT      UINT8_C(7)          // bracket literal, <len> <chr> ...
#define TOK_OCTLIT      UINT8_C(8)          // octal literal, <len> <digchr> ...
#define TOK_BRCLIT      UINT8_C(9)          // brace literal <len> <chr> ...
#define TOK_DECLIT      UINT8_C(10)         // decimal literal, <len> <digchr> ...
#define TOK_PRINT       UINT8_C(11)         // PRINT keyword
#define TOK_INPUT       UINT8_C(12)         // INPUT keyword
#define TOK_PUT         UINT8_C(13)         // PUT keyword
#define TOK_GET         UINT8_C(14)         // GET keyword
#define TOK_LIST        UINT8_C(15)         // LIST keyword
#define TOK_HEXLIT      UINT8_C(16)         // hexadecimal literal, <len> <digchr> ...
#define TOK_READ        UINT8_C(17)         // READ keyword
#define TOK_DATA        UINT8_C(18)         // DATA keyword
#define TOK_RESTORE     UINT8_C(19)         // RESTORE keyword
#define TOK_SAVE        UINT8_C(20)         // SAVE keyword
#define TOK_RUN         UINT8_C(21)         // RUN keyword
#define TOK_AUTO        UINT8_C(22)         // AUTO keyword
#define TOK_RENUM       UINT8_C(23)         // RENUM keyword
#define TOK_DELETE      UINT8_C(24)         // DELETE keyword
#define TOK_MERGE       UINT8_C(25)         // MERGE keyword
#define TOK_CHAIN       UINT8_C(26)         // CHAIN keyword
#define TOK_FILES       UINT8_C(27)         // FILES keyword
#define TOK_NEW         UINT8_C(28)         // NEW keyword
#define TOK_CLEAR       UINT8_C(29)         // CLEAR keyword
#define TOK_ERASE       UINT8_C(30)         // ERASE keyword
#define TOK_EDIT        UINT8_C(31)         // EDIT keyword
#define TOK_SPACE       UINT8_C(32)         // space
#define TOK_PLING       UINT8_C(33)         // ! pling
#define TOK_LATTICE     UINT8_C(35)         // # lattice
#define TOK_STRING      UINT8_C(36)         // $ string type sigil
#define TOK_INTEGER     UINT8_C(37)         // % integer type sigil
#define TOK_AMP         UINT8_C(38)         // & ampersand
#define TOK_QUOTE       UINT8_C(39)         // ' quote (comment)
#define TOK_LPAREN      UINT8_C(40)         // ( left parenthesis
#define TOK_RPAREN      UINT8_C(41)         // ) right parenthesis
#define TOK_MULT        UINT8_C(42)         // * operator
#define TOK_PLUS        UINT8_C(43)         // + operator
#define TOK_COMMA       UINT8_C(44)         // , comma
#define TOK_MINUS       UINT8_C(45)         // - operator
#define TOK_DIV         UINT8_C(47)         // / operator
#define TOK_DEC0        UINT8_C(48)         // decimal 0
#define TOK_DEC1        UINT8_C(49)         // decimal 1
#define TOK_DEC2        UINT8_C(50)         // decimal 2
#define TOK_DEC3        UINT8_C(51)         // decimal 3
#define TOK_DEC4        UINT8_C(52)         // decimal 4
#define TOK_DEC5        UINT8_C(53)         // decimal 5
#define TOK_DEC6        UINT8_C(54)         // decimal 6
#define TOK_DEC7        UINT8_C(55)         // decimal 7
#define TOK_DEC8        UINT8_C(56)         // decimal 8
#define TOK_DEC9        UINT8_C(57)         // decimal 9
#define TOK_COLON       UINT8_C(58)         // : colon
#define TOK_SEMIC       UINT8_C(59)         // ; semicolon
#define TOK_LT          UINT8_C(60)         // < operator
#define TOK_EQ          UINT8_C(61)         // = operator
#define TOK_GT          UINT8_C(62)         // > operator
#define TOK_LOAD        UINT8_C(63)         // LOAD keyword
#define TOK_ADDROF      UINT8_C(64)         // @ address-of operator
#define TOK_LE          UINT8_C(65)         // <= operator
#define TOK_NE          UINT8_C(66)         // <> not-equal operator
#define TOK_GE          UINT8_C(67)         // >= operator
#define TOK_SHOW        UINT8_C(68)         // SHOW keyword
#define TOK_WARRANTY    UINT8_C(69)         // WARRANTY keyword
#define TOK_COPYING     UINT8_C(70)         // COPYING keyword
#define TOK_DIM         UINT8_C(71)         // DIM keyword
#define TOK_DEF         UINT8_C(72)         // DEF keyword
#define TOK_INT         UINT8_C(73)         // INT keyword
#define TOK_STR         UINT8_C(74)         // STR keyword
#define TOK_FLT         UINT8_C(75)         // FLT keyword
#define TOK_OPTION      UINT8_C(76)         // OPTION keyword
#define TOK_BASE        UINT8_C(77)         // BASE keyword
#define TOK_ASC         UINT8_C(78)         // ASC keyword
#define TOK_VAL         UINT8_C(79)         // VAL keyword
#define TOK_LEFT        UINT8_C(80)         // LEFT keyword
#define TOK_MID         UINT8_C(81)         // MID keyword
#define TOK_RIGHT       UINT8_C(82)         // RIGHT keyword
#define TOK_INKEY       UINT8_C(83)         // INKEY keyword
#define TOK_BIN         UINT8_C(84)         // BIN keyword
#define TOK_QUA         UINT8_C(85)         // QUA keyword
#define TOK_OCT         UINT8_C(86)         // OCT keyword
#define TOK_DEC         UINT8_C(87)         // DEC keyword
#define TOK_HEX         UINT8_C(88)         // HEX keyword
#define TOK_GOTO        UINT8_C(89)         // GOTO keyword
#define TOK_GOSUB       UINT8_C(90)         // GOSUB keyword
#define TOK_LBRACK      UINT8_C(91)         // [ left bracket
#define TOK_BACKSL      UINT8_C(92)         // \ operator (integer division)
#define TOK_RBRACK      UINT8_C(93)         // ] right bracket
#define TOK_POW         UINT8_C(94)         // ^ operator (*)
#define TOK_GO          UINT8_C(95)         // GO keyword
#define TOK_BACKTK      UINT8_C(96)         // ` back tick (shell operator)
#define TOK_TO          UINT8_C(97)         // TO keyword
#define TOK_SUB         UINT8_C(98)         // SUB keyword
#define TOK_RETURN      UINT8_C(99)         // RETURN keyword
#define TOK_IF          UINT8_C(100)        // IF keyword
#define TOK_UNLESS      UINT8_C(101)        // UNLESS keyword
#define TOK_THEN        UINT8_C(102)        // THEN keyword
#define TOK_ELSE        UINT8_C(103)        // ELSE keyword
#define TOK_ENDIF       UINT8_C(104)        // ENDIF keyword
#define TOK_ENDUNLESS   UINT8_C(105)        // ENDUNLESS keyword
#define TOK_END         UINT8_C(106)        // END keyword
#define TOK_FOR         UINT8_C(107)        // FOR keyword
#define TOK_STEP        UINT8_C(108)        // STEP keyword
#define TOK_NEXT        UINT8_C(109)        // NEXT keyword
#define TOK_REPEAT      UINT8_C(110)        // REPEAT keyword
#define TOK_WHILE       UINT8_C(111)        // WHILE keyword
#define TOK_UNTIL       UINT8_C(112)        // UNTIL keyword
#define TOK_WEND        UINT8_C(113)        // WEND keyword
#define TOK_UEND        UINT8_C(114)        // UEND keyword
#define TOK_POP         UINT8_C(115)        // POP keyword
#define TOK_AFTER       UINT8_C(116)        // AFTER keyword
#define TOK_EVERY       UINT8_C(117)        // EVERY keyword
#define TOK_ON          UINT8_C(118)        // ON keyword
#define TOK_OFF         UINT8_C(119)        // OFF keyword
#define TOK_SYMBOL      UINT8_C(120)        // SYMBOL keyword
#define TOK_FN          UINT8_C(121)        // FN keyword
#define TOK_LET         UINT8_C(122)        // LET keyword
#define TOK_LBRACE      UINT8_C(123)        // { left brace
#define TOK_COLUMN      UINT8_C(124)        // | column
#define TOK_RBRACE      UINT8_C(125)        // } right brace
#define TOK_TILDE       UINT8_C(126)        // ~ tilde
#define TOK_NOT         UINT8_C(127)        // NOT operator
#define TOK_AND         UINT8_C(128)        // AND operator
#define TOK_OR          UINT8_C(129)        // OR operator
#define TOK_XOR         UINT8_C(130)        // XOR operator
#define TOK_NAND        UINT8_C(131)        // NAND operator
#define TOK_NOR         UINT8_C(132)        // NOR operator
#define TOK_XNOR        UINT8_C(133)        // XNOR operator
#define TOK_LSHIFT      UINT8_C(134)        // LSHIFT operator
#define TOK_RSHIFT      UINT8_C(135)        // RSHIFT operator
#define TOK_DYNAMIC     UINT8_C(136)        // DYNAMIC keyword
#define TOK_ASSOC       UINT8_C(137)        // ASSOC keyword
#define TOK_NUMIDENT    UINT8_C(138)        // numeric identifier
#define TOK_STRIDENT    UINT8_C(139)        // string identifier
#define TOK_INTIDENT    UINT8_C(140)        // integer identifier
#define TOK_FRE         UINT8_C(141)        // FRE function
#define TOK_TI          UINT8_C(142)        // TI function
#define TOK_LABEL       UINT8_C(143)        // LABEL keyword
#define TOK_LEN         UINT8_C(144)        // LEN function
#define TOK_AGAIN       UINT8_C(145)        // AGAIN keyword
#define TOK_CHR         UINT8_C(146)        // CHR keyword
#define TOK_MONITOR     UINT8_C(147)        // MONITOR keyword
#define TOK_SOUND       UINT8_C(148)        // SOUND keyword
#define TOK_PLAY        UINT8_C(149)        // PLAY keyword
#define TOK_WAVE        UINT8_C(150)        // WAVE keyword
#define TOK_TEMPO       UINT8_C(151)        // TEMPO keyword
#define TOK_PLAYING     UINT8_C(152)        // PLAYING function
#define TOK_SYNC        UINT8_C(153)        // SYNC keyword (SOUND SYNC)
#define TOK_PITCH       UINT8_C(154)        // PITCH keyword
#define TOK_VOLUME      UINT8_C(155)        // VOLUME keyword
#define TOK_ENVELOPE    UINT8_C(156)        // ENVELOPE keyword
#define TOK_VIBRATO     UINT8_C(157)        // VIBRATO keyword
#define TOK_TREMOLO     UINT8_C(158)        // TREMOLO keyword
#define TOK_SFX         UINT8_C(159)        // SFX keyword
#define TOK_WAIT        UINT8_C(160)        // WAIT keyword
#define TOK_REMAIN      UINT8_C(161)        // REMAIN function
#define TOK_OPEN        UINT8_C(162)        // OPEN keyword
#define TOK_CLOSE       UINT8_C(163)        // CLOSE keyword
#define TOK_LINE        UINT8_C(164)        // LINE keyword (LINE INPUT)
#define TOK_WRITE       UINT8_C(165)        // WRITE keyword
#define TOK_ADDRESS     UINT8_C(166)        // ADDRESS keyword and ADDRESS$ function
#define TOK_EOF         UINT8_C(167)        // EOF function
#define TOK_LOF         UINT8_C(168)        // LOF function
#define TOK_LOC         UINT8_C(169)        // LOC function
#define TOK_KILL        UINT8_C(170)        // KILL keyword
#define TOK_RENAME      UINT8_C(171)        // RENAME keyword
#define TOK_MKDIR       UINT8_C(172)        // MKDIR keyword
#define TOK_CHDIR       UINT8_C(173)        // CHDIR keyword
#define TOK_SEEK        UINT8_C(174)        // SEEK keyword
#define TOK_FIELD       UINT8_C(175)        // FIELD keyword
#define TOK_LSET        UINT8_C(176)        // LSET keyword
#define TOK_RSET        UINT8_C(177)        // RSET keyword
#define TOK_GETREC      UINT8_C(178)        // GETREC keyword
#define TOK_PUTREC      UINT8_C(179)        // PUTREC keyword
#define TOK_ERR         UINT8_C(180)        // ERR$ function
#define TOK_ERL         UINT8_C(181)        // ERL function
#define TOK_MKI         UINT8_C(182)        // MKI$ function (16 bit integer to a 2 byte string)
#define TOK_MKL         UINT8_C(183)        // MKL$ function (32 bit integer, 4 bytes)
#define TOK_MKS         UINT8_C(184)        // MKS$ function (single precision float, 4 bytes)
#define TOK_MKD         UINT8_C(185)        // MKD$ function (double precision float, 8 bytes)
#define TOK_CVI         UINT8_C(186)        // CVI function (2 byte string to a number)
#define TOK_CVL         UINT8_C(187)        // CVL function
#define TOK_CVS         UINT8_C(188)        // CVS function
#define TOK_CVD         UINT8_C(189)        // CVD function
#define TOK_GFX         UINT8_C(190)        // GFX keyword (drawing, layers, palettes)
#define TOK_SPRITE      UINT8_C(191)        // SPRITE keyword
#define TOK_TILE        UINT8_C(192)        // TILE keyword
#define TOK_TXT         UINT8_C(193)        // TXT keyword (text screen areas)
#define TOK_DESIGN      UINT8_C(194)        // DESIGN keyword
#define TOK_RGB         UINT8_C(195)        // RGB function
#define TOK_POINT       UINT8_C(196)        // POINT function
#define TOK_CLS         UINT8_C(197)        // CLS keyword
#define TOK_LOCATE      UINT8_C(198)        // LOCATE keyword
#define TOK_COLOR       UINT8_C(199)        // COLOR keyword
#define TOK_MOD         UINT8_C(200)        // MOD operator
#define TOK_ABS         UINT8_C(201)        // ABS function
#define TOK_SGN         UINT8_C(202)        // SGN function
#define TOK_SQR         UINT8_C(203)        // SQR function
#define TOK_SIN         UINT8_C(204)        // SIN function
#define TOK_COS         UINT8_C(205)        // COS function
#define TOK_TAN         UINT8_C(206)        // TAN function
#define TOK_ATN         UINT8_C(207)        // ATN function
#define TOK_EXP         UINT8_C(208)        // EXP function
#define TOK_LOG         UINT8_C(209)        // LOG function
#define TOK_RND         UINT8_C(210)        // RND function
#define TOK_FIX         UINT8_C(211)        // FIX function
#define TOK_PI          UINT8_C(212)        // PI function
#define TOK_RANDOMIZE   UINT8_C(213)        // RANDOMIZE keyword
#define TOK_NONE        UINT8_C(255)        // no token (!)

// -- buffers ---------------------------------------------------------------

/*
   buf_t is a growable output buffer. "fill" is the number of bytes in use.
   rbuf_t is a bounded read-only view. All eat_ functions scan text and rely on
   the bytes up to "size" only; they never modify rpos when they fail.
   Strings returned through "char**" parameters are heap allocated: the callee frees
   the previous value (so initialize to 0), and the caller frees the final one.
*/

typedef struct _buf_t {
      char*       buffer;
      size_t      alloc;
      size_t      fill;
} buf_t;

#define BUF_INIT_SIZE   65536U

buf_t* create_buffer( void );
void delete_buffer( buf_t* buf );
void clear_buffer( buf_t* buf );
bool grow_buffer( buf_t* buf, size_t requiredSize );      // ensures alloc >= requiredSize
int printto_buffer( buf_t* buf, const char* fmt, ... );
int writeto_buffer( buf_t* buf, const void* data, size_t size );
int anticipate_buffer( buf_t* buf, size_t size );         // ensures room for size more bytes

// -- read-only buffers -----------------------------------------------------

typedef struct _rbuf_t {
      const char* buffer;
      size_t      size;
      size_t      rpos;
} rbuf_t;

void init_rbuf( rbuf_t* rbuf, const void* data, size_t size );
int readfrom_buffer( rbuf_t* rbuf, void* data, size_t size );
int anticipate_rbuffer( rbuf_t* rbuf, size_t size );

// -- single character tokens -----------------------------------------------

bool is_sngchrtok( char tok );
bool eat_sngchrtok( rbuf_t* rbuf, uint8_t* ptok );

// -- raw 16-bit values (used by line headers) ------------------------------

void emit_uint16( uint8_t** pp, uint16_t source );
void read_uint16( const uint8_t** pp, uint16_t* target );

// -- raw 24-bit values (used for line numbers) -----------------------------

void emit_uint24( uint8_t** pp, uint32_t source );
void read_uint24( const uint8_t** pp, uint32_t* target );

// -- uint32 ----------------------------------------------------------------

bool eat_uint32( rbuf_t* rbuf, uint32_t* target );
bool print_uint32( buf_t* buf, uint32_t source );
bool emit_uint32( buf_t* buf, uint32_t source );
bool read_uint32( rbuf_t* rbuf, uint32_t* target );

// -- identifiers -----------------------------------------------------------

bool eat_ident( rbuf_t* rbuf, char** ptarget );
bool print_ident( buf_t* buf, const char* source );
bool emit_ident( buf_t* buf, const char* source, uint8_t tok );
extern bool ri_sigil;
bool read_ident( rbuf_t* rbuf, char** ptarget );

// -- literals (general) ----------------------------------------------------

bool eat_lit( rbuf_t* rbuf, char** ptarget, int beg, int end );
bool print_lit( buf_t* buf, const char* source, int beg, int end );
bool emit_lit( buf_t* buf, const char* source, int tok );
bool read_lit( rbuf_t* rbuf, char** ptarget, int tok );

// -- numeric literals (general) --------------------------------------------

bool eat_numlit( rbuf_t* rbuf, char** ptarget, int* pbase );
bool print_numlit( buf_t* buf, const char* source, int base );
bool emit_numlit( buf_t* buf, const char* source, int tok );
bool read_numlit( rbuf_t* rbuf, char** ptarget, int tok );

// -- decimal literals ------------------------------------------------------

bool print_declit( buf_t* buf, const char* source );
bool emit_declit( buf_t* buf, const char* source );
bool read_declit( rbuf_t* rbuf, char** ptarget );

// -- hexadecimal literals --------------------------------------------------

bool print_hexlit( buf_t* buf, const char* source );
bool emit_hexlit( buf_t* buf, const char* source );
bool read_hexlit( rbuf_t* rbuf, char** ptarget );

// -- octal literals --------------------------------------------------------

bool print_octlit( buf_t* buf, const char* source );
bool emit_octlit( buf_t* buf, const char* source );
bool read_octlit( rbuf_t* rbuf, char** ptarget );

// -- quaternary literals ---------------------------------------------------

bool print_qualit( buf_t* buf, const char* source );
bool emit_qualit( buf_t* buf, const char* source );
bool read_qualit( rbuf_t* rbuf, char** ptarget );

// -- binary literals -------------------------------------------------------

bool print_binlit( buf_t* buf, const char* source );
bool emit_binlit( buf_t* buf, const char* source );
bool read_binlit( rbuf_t* rbuf, char** ptarget );

// -- string literals -------------------------------------------------------

bool eat_strlit( rbuf_t* rbuf, char** ptarget );
bool print_strlit( buf_t* buf, const char* source );
bool emit_strlit( buf_t* buf, const char* source );
bool read_strlit( rbuf_t* rbuf, char** ptarget );

// -- shell literals --------------------------------------------------------

bool eat_shllit( rbuf_t* rbuf, char** ptarget );
bool print_shllit( buf_t* buf, const char* source );
bool emit_shllit( buf_t* buf, const char* source );
bool read_shllit( rbuf_t* rbuf, char** ptarget );

// -- quote literals (comments) ---------------------------------------------

bool eat_quolit( rbuf_t* rbuf, char** ptarget );
bool print_quolit( buf_t* buf, const char* source );
bool emit_quolit( buf_t* buf, const char* source );
bool read_quolit( rbuf_t* rbuf, char** ptarget );

// -- bracket literals ------------------------------------------------------

bool eat_brklit( rbuf_t* rbuf, char** ptarget );
bool print_brklit( buf_t* buf, const char* source );
bool emit_brklit( buf_t* buf, const char* source );
bool read_brklit( rbuf_t* rbuf, char** ptarget );

// -- brace literals --------------------------------------------------------

bool eat_brclit( rbuf_t* rbuf, char** ptarget );
bool print_brclit( buf_t* buf, const char* source );
bool emit_brclit( buf_t* buf, const char* source );
bool read_brclit( rbuf_t* rbuf, char** ptarget );

// -- keywords --------------------------------------------------------------

bool is_keyword( const char* name, uint8_t* ptok );
bool is_keyword2( uint8_t tok );
const char* keyword_name( uint8_t tok );        // the keyword text of a token, 0 if it is none
bool print_keyword( uint8_t tok, buf_t* buf );

// -- double-character tokens -----------------------------------------------

bool eat_dblchrtok( rbuf_t* rbuf, uint8_t* ptok, const char* match, uint8_t tok );

// -- tokenization ----------------------------------------------------------

struct _linehdr_t;

/*
   Appends one tokenized line (a linehdr_t followed by the tokens, terminated by TOK_EOL)
   to "out". On failure, "out" is left unchanged. "text" must be NUL-terminated.
   The line is limited to 65535 bytes in total (16-bit length field).
*/
bool tokenize_line( const char* text, buf_t* out, struct _linehdr_t* phdr );

// -- detokenization --------------------------------------------------------

/*
   Appends the source text of a tokenized line to "out". "wherefrom" points to the first
   token (after the line header); the token count is taken from phdr->length.
   The result is NUL-terminated, but the terminator is not counted in out->fill.
*/
bool detokenize_line( buf_t* out, const uint8_t* wherefrom, const struct _linehdr_t* phdr );

/*
   Syntax highlighting (optional). detokenize_line_hl() does the same as detokenize_line(), but calls "style" every time
   the class of the text changes and inserts the text it returns (an escape sequence, say) into the output before the
   new text; "usrdata" is passed through. The first call of a line is for the line number, the last one (HL_NONE) comes
   after the end of the line, if anything was written before it, so a host can put its "reset" there. Spaces keep the
   class of what precedes them. "style" may return 0 for "nothing to insert". With style == 0 this is detokenize_line().
   Colored text is for display only: the listings that are parsed again (labels, RENUM) and SAVE use the plain one.
*/
typedef enum _hlclass_t {
    HL_NONE,            // not a class: end of the line (reset)
    HL_LINENO,          // line number
    HL_STATEMENT,       // statement keywords: PRINT INPUT DIM DEF LET DATA ...
    HL_CONTROL,         // flow control: IF THEN ELSE FOR TO NEXT GOTO GOSUB RETURN REPEAT WHILE END ...
    HL_COMMAND,         // program commands: LIST RUN NEW LOAD SAVE DELETE RENUM CLEAR ...
    HL_FUNCTION,        // functions: LEN ASC VAL LEFT MID RIGHT CHR INKEY HEX BIN FN ...
    HL_OPERATOR,        // + - * / ^ = < > ... and AND OR NOT XOR ...
    HL_IDENT,           // variable names
    HL_NUMBER,          // numeric literals (all bases)
    HL_STRING,          // string literals (and bracket / brace literals)
    HL_SHELL,           // shell literals (backticks)
    HL_COMMENT,         // comment (' ...)
    HL_PUNCT,           // ( ) , : ; # $ % [ ] { } |
    HL_FILEDIR,         // FILES: directory
    HL_FILELINK,        // FILES: symbolic link
    HL_FILEEXEC,        // FILES: executable file
    HL_FILEPROG,        // FILES: BASIC program file (.bas, .e99)
    HL_COUNT
} hlclass_t;

typedef const char* (*hlstyle_t)( hlclass_t cls, void* usrdata );

bool detokenize_line_hl( buf_t* out, const uint8_t* wherefrom, const struct _linehdr_t* phdr,
    hlstyle_t style, void* usrdata );

// -- buffer preprocessing --------------------------------------------------

void preprocess_buffer( char* buf );

#endif
