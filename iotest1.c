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

#include "iodev.h"

/*
Automated test of the I/O subsystem layer (iodev.c) without a screen or a VM: name resolution, the null, mem and file drivers, whole
files and the channel table. Exit status = number of failures.
*/

static int checks = 0, failures = 0;

static void check( const char* name, bool ok ) {
    ++checks;
    if ( !ok ) {
        ++failures;
        printf( "FAIL  %s\n", name );
    }
}

static bool open_chan( iotab_t* t, int ch, const char* name, unsigned mode, const char** perr ) {
    return iotab_open( t, ch, name, "file", mode, perr );
}

static void test_resolve( void ) {
    iodrv_t* d = 0; const char* path = 0; const char* e = 0;
    check( "plain name -> default driver", iodev_resolve( "foo.txt", "file", &d, &path, &e ) && d == iodev_find( "file" ) && strcmp( path, "foo.txt" ) == 0 );
    check( "mem:name", iodev_resolve( "mem:scores", "file", &d, &path, &e ) && d == iodev_find( "mem" ) && strcmp( path, "scores" ) == 0 );
    check( "prefix is case insensitive", iodev_resolve( "MEM:x", "file", &d, &path, &e ) && d == iodev_find( "mem" ) );
    check( "the default driver is used without a prefix", iodev_resolve( "x", "mem", &d, &path, &e ) && d == iodev_find( "mem" ) );
    check( "an explicit prefix wins", iodev_resolve( "file:x", "mem", &d, &path, &e ) && d == iodev_find( "file" ) );
    check( "unknown driver is an error", !iodev_resolve( "nodev:x", "file", &d, &path, &e ) && strcmp( e, "Device not found" ) == 0 );
    check( "a one letter prefix is no driver", iodev_resolve( "c:x", "file", &d, &path, &e ) && d == iodev_find( "file" ) && strcmp( path, "c:x" ) == 0 );
    check( "a prefix has to start with a letter", iodev_resolve( "1mem:x", "file", &d, &path, &e ) && strcmp( path, "1mem:x" ) == 0 );
    check( "a colon later in the name is no prefix", iodev_resolve( "dir/a:b", "file", &d, &path, &e ) && strcmp( path, "dir/a:b" ) == 0 );
    check( "file: forces a disk file with a colon", iodev_resolve( "file:a:b", "mem", &d, &path, &e ) && d == iodev_find( "file" ) && strcmp( path, "a:b" ) == 0 );
    check( "empty path after the prefix", iodev_resolve( "mem:", "file", &d, &path, &e ) && path[0] == '\0' );
    check( "unknown default driver", !iodev_resolve( "x", "nodev", &d, &path, &e ) );
    check( "a name that is too long is no prefix", iodev_resolve( "abcdefghijklmnopqr:x", "file", &d, &path, &e ) && strcmp( path, "abcdefghijklmnopqr:x" ) == 0 );
}

static void test_mem_whole( void ) {
    const char* e = 0; uint8_t* data = 0; size_t len = 0;
    iodev_mem_clear();
    check( "write a whole mem file", iodev_write_all( "mem:a.txt", "file", "hello", 5, &e ) );
    check( "it exists", iodev_exists( "mem:a.txt", "file" ) );
    check( "another one does not", !iodev_exists( "mem:b.txt", "file" ) );
    check( "read it back", iodev_read_all( "mem:a.txt", "file", &data, &len, &e ) && len == 5 && memcmp( data, "hello", 5 ) == 0 );
    free( data );
    check( "read a missing file", !iodev_read_all( "mem:nothing", "file", &data, &len, &e ) && strcmp( e, "File not found" ) == 0 );
    check( "rewriting truncates", iodev_write_all( "mem:a.txt", "file", "x", 1, &e ) && iodev_read_all( "mem:a.txt", "file", &data, &len, &e ) && len == 1 );
    free( data );
    check( "an empty file", iodev_write_all( "mem:empty", "file", "", 0, &e ) && iodev_read_all( "mem:empty", "file", &data, &len, &e ) && len == 0 );
    free( data );
    check( "a large file", true );
    {
        size_t big = 300000;
        uint8_t* buf = (uint8_t*) malloc( big );
        for ( size_t i=0; i < big; ++i ) buf[i] = (uint8_t)( i * 7U );
        bool ok = iodev_write_all( "mem:big", "file", buf, big, &e ) && iodev_read_all( "mem:big", "file", &data, &len, &e ) && len == big &&
            memcmp( data, buf, big ) == 0;
        check( "300000 bytes round trip", ok );
        free( buf ); free( data );
    }
    iodev_mem_clear();
}

static void test_channels_mem( void ) {
    iotab_t t; iotab_init( &t );
    const char* e = 0;
    iodev_mem_clear();
    check( "open for output", iotab_open( &t, 1, "mem:f", "file", IOMODE_WRITE, &e ) );
    check( "open again is an error", !iotab_open( &t, 1, "mem:g", "file", IOMODE_WRITE, &e ) && strcmp( e, "File already open" ) == 0 );
    check( "channel 0 can't be opened", !iotab_open( &t, 0, "mem:g", "file", IOMODE_WRITE, &e ) && strcmp( e, "Bad file number" ) == 0 );
    check( "channel 256 can't be opened", !iotab_open( &t, 256, "mem:g", "file", IOMODE_WRITE, &e ) );
    check( "write lines", iotab_write( &t, 1, "one\ntwo\r\nthree", 15, &e ) );
    int64_t n = 0;
    check( "LOC of an output file", iotab_loc( &t, 1, &n, &e ) && n == 15 );
    check( "LOF of an output file", iotab_lof( &t, 1, &n, &e ) && n == 15 );
    check( "reading from an output file is a bad mode", iotab_getc( &t, 1, &e ) == -2 && strcmp( e, "Bad file mode" ) == 0 );
    check( "close", iotab_close( &t, 1, &e ) );
    check( "closing a closed channel is fine", iotab_close( &t, 1, &e ) );
    check( "writing to a closed channel", !iotab_write( &t, 1, "x", 1, &e ) && strcmp( e, "File not open" ) == 0 );

    check( "open for input", iotab_open( &t, 2, "mem:f", "file", IOMODE_READ, &e ) );
    char* line = 0;
    bool eof = true;
    check( "not at the end", iotab_eof( &t, 2, &eof, &e ) && !eof );
    check( "writing to an input file is a bad mode", !iotab_write( &t, 2, "x", 1, &e ) && strcmp( e, "Bad file mode" ) == 0 );
    check( "line 1", iotab_readline( &t, 2, &line, &e ) && strcmp( line, "one" ) == 0 );
    free( line );
    check( "line 2 loses the CR", iotab_readline( &t, 2, &line, &e ) && strcmp( line, "two" ) == 0 );
    free( line );
    check( "LOC counts the bytes read", iotab_loc( &t, 2, &n, &e ) && n == 9 );
    check( "a last line without a newline", iotab_readline( &t, 2, &line, &e ) && strcmp( line, "three" ) == 0 );
    free( line );
    check( "now at the end", iotab_eof( &t, 2, &eof, &e ) && eof );
    check( "no more lines (no error)", !iotab_readline( &t, 2, &line, &e ) && e == 0 );
    check( "getc at the end", iotab_getc( &t, 2, &e ) == -1 );
    check( "LOF of an input file", iotab_lof( &t, 2, &n, &e ) && n == 15 );
    iotab_close( &t, 2, &e );

    check( "append", iotab_open( &t, 3, "mem:f", "file", IOMODE_WRITE | IOMODE_APPEND, &e ) && iotab_write( &t, 3, "\nfour\n", 6, &e ) );
    check( "LOC of an append file starts at the end", iotab_loc( &t, 3, &n, &e ) && n == 21 );
    iotab_close( &t, 3, &e );
    uint8_t* data = 0; size_t len = 0;
    check( "appended text", iodev_read_all( "mem:f", "file", &data, &len, &e ) && len == 21 && memcmp( data + 15, "\nfour\n", 6 ) == 0 );
    free( data );
    check( "open a missing file for input", !iotab_open( &t, 4, "mem:nothing", "file", IOMODE_READ, &e ) && strcmp( e, "File not found" ) == 0 );
    check( "open on an unknown device", !iotab_open( &t, 4, "nodev:x", "file", IOMODE_READ, &e ) && strcmp( e, "Device not found" ) == 0 );
    check( "the failed opens left no channel", !iotab_isopen( &t, 4 ) );

    check( "the empty line", iotab_open( &t, 5, "mem:lines", "file", IOMODE_WRITE, &e ) && iotab_write( &t, 5, "\n\nx\n", 4, &e ) );
    iotab_close( &t, 5, &e );
    iotab_open( &t, 5, "mem:lines", "file", IOMODE_READ, &e );
    bool ok = iotab_readline( &t, 5, &line, &e ) && line[0] == '\0';
    free( line );
    ok = ok && iotab_readline( &t, 5, &line, &e ) && line[0] == '\0';
    free( line );
    ok = ok && iotab_readline( &t, 5, &line, &e ) && strcmp( line, "x" ) == 0;
    free( line );
    check( "empty lines are lines", ok );
    check( "closeall", ( iotab_closeall( &t ), !iotab_isopen( &t, 5 ) ) );

    // a long line over the buffer size
    {
        size_t n2 = 10000;
        char* big = (char*) malloc( n2 + 2U );
        memset( big, 'a', n2 ); big[n2] = '\n'; big[n2 + 1U] = 'b';
        iodev_write_all( "mem:long", "file", big, n2 + 2U, &e );
        iotab_open( &t, 1, "mem:long", "file", IOMODE_READ, &e );
        bool ok2 = iotab_readline( &t, 1, &line, &e ) && strlen( line ) == n2;
        free( line );
        ok2 = ok2 && iotab_readline( &t, 1, &line, &e ) && strcmp( line, "b" ) == 0;
        free( line );
        check( "a line longer than the read buffer", ok2 );
        free( big );
        iotab_closeall( &t );
    }
    // a file that is removed while it is open stays readable
    iodev_write_all( "mem:gone", "file", "abc", 3, &e );
    iotab_open( &t, 1, "mem:gone", "file", IOMODE_READ, &e );
    iodev_mem_clear();
    check( "a removed mem file stays readable while open", iotab_getc( &t, 1, &e ) == 'a' );
    iotab_closeall( &t );
    iodev_mem_clear();
}

static void test_null( void ) {
    iotab_t t; iotab_init( &t );
    const char* e = 0;
    check( "open null for output", iotab_open( &t, 1, "null:", "file", IOMODE_WRITE, &e ) && iotab_write( &t, 1, "anything", 8, &e ) );
    iotab_close( &t, 1, &e );
    bool eof = false;
    check( "null is always at the end", iotab_open( &t, 2, "null:", "file", IOMODE_READ, &e ) && iotab_eof( &t, 2, &eof, &e ) && eof );
    iotab_closeall( &t );
}

static void test_file( void ) {
    iotab_t t; iotab_init( &t );
    const char* e = 0;
    const char* name = "iotest1.tmp";
    remove( name );
    check( "file: output", open_chan( &t, 1, name, IOMODE_WRITE, &e ) && iotab_write( &t, 1, "disk\nfile\n", 10, &e ) );
    int64_t n = 0;
    check( "LOF flushes", iotab_lof( &t, 1, &n, &e ) && n == 10 );
    iotab_close( &t, 1, &e );
    char* line = 0;
    check( "file: input", open_chan( &t, 2, name, IOMODE_READ, &e ) && iotab_readline( &t, 2, &line, &e ) && strcmp( line, "disk" ) == 0 );
    free( line );
    iotab_close( &t, 2, &e );
    check( "file: append", open_chan( &t, 3, name, IOMODE_WRITE | IOMODE_APPEND, &e ) && iotab_loc( &t, 3, &n, &e ) && n == 10 );
    iotab_closeall( &t );
    check( "file: missing", !open_chan( &t, 1, "iotest1.nothing", IOMODE_READ, &e ) && strcmp( e, "File not found" ) == 0 );
    check( "file: a directory", !open_chan( &t, 1, ".", IOMODE_READ, &e ) && strcmp( e, "Is a directory" ) == 0 );
    check( "file: no such directory for output", !open_chan( &t, 1, "no/such/dir/x", IOMODE_WRITE, &e ) );
    check( "file: exists", iodev_exists( name, "file" ) && !iodev_exists( ".", "file" ) );
    check( "file: prefix", iodev_exists( "file:iotest1.tmp", "mem" ) );
    iodrv_t* fd = iodev_find( "file" );
    char err[80];
    check( "file: remove", fd->remove( fd, name, err, sizeof(err) ) && !iodev_exists( name, "file" ) );
    check( "file: remove again", !fd->remove( fd, name, err, sizeof(err) ) );
}

static bool collect( void* ud, const char* line ) {
    char* buf = (char*) ud;
    strcat( buf, line );
    strcat( buf, "\n" );
    return true;
}

static void test_list( void ) {
    const char* e = 0;
    char err[80], out[2000];
    iodev_mem_clear();
    iodev_write_all( "mem:b.txt", "file", "12345", 5, &e );
    iodev_write_all( "mem:a.bas", "file", "10", 2, &e );
    out[0] = '\0';
    check( "list mem:", iodev_list( "mem:", "file", 0, 0, collect, out, err, sizeof(err) ) );
    check( "both files are listed", strstr( out, "b.txt" ) != 0 && strstr( out, "a.bas" ) != 0 && strstr( out, "5 20" ) != 0 );
    out[0] = '\0';
    check( "list a pattern", iodev_list( "mem:*.bas", "file", 0, 0, collect, out, err, sizeof(err) ) && strstr( out, "a.bas" ) != 0 && strstr( out, "b.txt" ) == 0 );
    check( "list without a match", !iodev_list( "mem:*.zzz", "file", 0, 0, collect, out, err, sizeof(err) ) && strcmp( err, "File not found" ) == 0 );
    check( "list on an unknown device", !iodev_list( "nodev:", "file", 0, 0, collect, out, err, sizeof(err) ) && strcmp( err, "Device not found" ) == 0 );
    check( "null can't list", !iodev_list( "null:", "file", 0, 0, collect, out, err, sizeof(err) ) && strcmp( err, "Not supported by device" ) == 0 );
    iodev_mem_clear();
}

static bool user_ok = false;
static void* user_open( iodrv_t* d, const char* p, unsigned m, char* e, size_t n ) { (void) d; (void) p; (void) m; (void) e; (void) n; user_ok = true; return malloc( 1 ); }
static ssize_t user_read( void* h, void* b, size_t n ) { (void) h; (void) b; (void) n; return 0; }
static bool user_close( void* h ) { free( h ); return true; }
static iodrv_t user_driver = { "usr_test", user_open, user_read, 0, 0, user_close, 0, 0, 0 };

static void test_register( void ) {
    check( "register a driver", iodev_register( &user_driver ) );
    check( "register twice", !iodev_register( &user_driver ) );
    iodrv_t bad = { "x", user_open, 0, 0, 0, 0, 0, 0, 0 };
    check( "a name of one character is invalid", !iodev_register( &bad ) );
    iotab_t t; iotab_init( &t );
    const char* e = 0;
    check( "use the registered driver", iotab_open( &t, 1, "usr_test:anything", "file", IOMODE_READ, &e ) && user_ok );
    check( "a driver without write is a bad mode", !iotab_write( &t, 1, "x", 1, &e ) );
    check( "a driver without size", ( iotab_open( &t, 2, "usr_test:x", "file", IOMODE_READ, &e ), true ) );
    int64_t n = 0;
    check( "LOF without size is not supported", !iotab_lof( &t, 2, &n, &e ) && strcmp( e, "Not supported by device" ) == 0 );
    iotab_closeall( &t );
}

int main( void ) {
    iodev_init();
    test_resolve();
    test_mem_whole();
    test_channels_mem();
    test_null();
    test_file();
    test_list();
    test_register();
    printf( "%d checks, %d failure(s)\n", checks, failures );
    return failures;
}
