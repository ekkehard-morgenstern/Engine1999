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

#ifndef IODEV_H
#define IODEV_H     1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

#ifndef BASFILES_H
#include "basfiles.h"
#endif

/*
The I/O subsystem (docs/io-subsystem.md): virtual device drivers addressed by "driver:" in front of a file name, and the table of
the numbered channels (#1 ...) that OPEN fills. No SDL, no VM: the VM and the REPL use it through iotab_* and iodev_*.
All functions that can fail report a short message in "const char** perr" (a static text, e.g. "File not found").
*/

#define IOMODE_READ     1U          // OPEN FOR INPUT
#define IOMODE_WRITE    2U          // OPEN FOR OUTPUT (truncates)
#define IOMODE_APPEND   4U          // OPEN FOR APPEND (with IOMODE_WRITE)

#define IOCHAN_MAX      255         // channels 1 .. IOCHAN_MAX can be opened; channel 0 is the console

typedef struct _iodrv_t {
    const char* name;               // "file"; matched without regard to case
    // opens the part of the name behind the prefix; returns a handle, 0 with a message in err
    void*       (*open)( struct _iodrv_t* drv, const char* path, unsigned mode, char* err, size_t errsz );
    ssize_t     (*read)( void* handle, void* buf, size_t count );       // 0 = end of the data, < 0 = error
    ssize_t     (*write)( void* handle, const void* buf, size_t count );
    int64_t     (*size)( void* handle );                                // the length in bytes, < 0 = unknown
    bool        (*close)( void* handle );
    // FILES: lines about the entries that match spec ("" = all); same contract as files_list()
    bool        (*list)( struct _iodrv_t* drv, const char* spec, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud,
                    char* err, size_t errsz );
    // deletes a file (KILL, phase 2; also used by the tests)
    bool        (*remove)( struct _iodrv_t* drv, const char* path, char* err, size_t errsz );
    void*       userdata;
} iodrv_t;

// -- the drivers -------------------------------------------------------------------------

void iodev_init( void );                        // registers file, null and mem (once)
bool iodev_register( iodrv_t* drv );            // the driver must stay valid; false if the name is taken or invalid
iodrv_t* iodev_find( const char* name );        // 0 if there is no such driver

// Splits "driver:path" (a prefix of 2 to 16 letters / digits / underscores starting with a letter and a colon). Without a prefix
// the driver is defaultdriver. false with "Device not found" for a prefix that is no driver.
bool iodev_resolve( const char* name, const char* defaultdriver, iodrv_t** pdrv, const char** ppath, const char** perr );

// the one place where an access policy can be added (sandboxing, docs/io-subsystem.md); allows everything for now
bool iodev_allowed( const char* operation, const iodrv_t* drv, const char* path );

// whole files (LOAD, MERGE, CHAIN read; SAVE writes)
bool iodev_read_all( const char* name, const char* defaultdriver, uint8_t** pdata, size_t* plen, const char** perr );
bool iodev_write_all( const char* name, const char* defaultdriver, const void* data, size_t len, const char** perr );
bool iodev_exists( const char* name, const char* defaultdriver );
bool iodev_list( const char* spec, const char* defaultdriver, hlstyle_t style, void* styleud, files_emit_t emit, void* emitud,
    char* err, size_t errsz );

// the RAM disk, for the tests: deletes all files of the mem driver
void iodev_mem_clear( void );

// -- channels ------------------------------------------------------------------------------

typedef struct _iochan_t {
    iodrv_t*    drv;
    void*       handle;
    unsigned    mode;
    uint8_t*    rbuf;                       // read buffer
    size_t      rpos, rlen;
    int64_t     pos;                        // bytes read / written so far (LOC)
} iochan_t;

typedef struct _iotab_t {
    iochan_t*   chan[ IOCHAN_MAX + 1 ];     // 0 is never used
} iotab_t;

void iotab_init( iotab_t* tab );
void iotab_closeall( iotab_t* tab );
bool iotab_isopen( const iotab_t* tab, int chan );
bool iotab_open( iotab_t* tab, int chan, const char* name, const char* defaultdriver, unsigned mode, const char** perr );
bool iotab_close( iotab_t* tab, int chan, const char** perr );

// input: for channels opened FOR INPUT; output: for OUTPUT / APPEND
int iotab_getc( iotab_t* tab, int chan, const char** perr );                    // 0..255, -1 at the end, -2 on an error
bool iotab_readline( iotab_t* tab, int chan, char** pline, const char** perr);  // malloc'd line without "\n" / "\r\n"; false at the end or on an error (*perr == 0 at the end)
bool iotab_write( iotab_t* tab, int chan, const char* data, size_t len, const char** perr );
bool iotab_eof( iotab_t* tab, int chan, bool* peof, const char** perr );
bool iotab_lof( iotab_t* tab, int chan, int64_t* plen, const char** perr );
bool iotab_loc( iotab_t* tab, int chan, int64_t* ppos, const char** perr );

#endif
