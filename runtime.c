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

#include "runtime.h"

static void rt_vmem_report( struct _varmem_t* vmem, void* usrdata, const char* text ) {
    runtime_t* rt = (runtime_t*) usrdata;
    if ( rt->report ) {
        rt->report( rt, rt->userdata, text );
    } else {
        fprintf( stderr, "%s", text );
    }
}

static void rt_vmem_halt( struct _varmem_t*, void* ) ATTR_NORETURN;

static void rt_vmem_halt( struct _varmem_t* vmem, void* usrdata ) {
    runtime_t* rt = (runtime_t*) usrdata;
    if ( rt->halt ) {
        rt->halt( rt, rt->userdata );
    } else {
        exit( EXIT_FAILURE );
    }
}

static void rt_cgen_report( struct _codegen_t* cgen, void* usrdata, const char* text ) {
    runtime_t* rt = (runtime_t*) usrdata;
    if ( rt->report ) {
        rt->report( rt, rt->userdata, text );
    } else {
        fprintf( stderr, "%s", text );
    }
}

static void rt_cgen_halt( struct _codegen_t*, void* ) ATTR_NORETURN;

static void rt_cgen_halt( struct _codegen_t* cgen, void* usrdata ) {
    runtime_t* rt = (runtime_t*) usrdata;
    if ( rt->halt ) {
        rt->halt( rt, rt->userdata );
    } else {
        exit( EXIT_FAILURE );
    }
}

void init_runtime( runtime_t* rt ) {

    init_varmem( &rt->varmem );
    rt->varmem.userdata = rt;
    rt->varmem.report   = rt_vmem_report;
    rt->varmem.halt     = rt_vmem_halt;

    init_codegen( &rt->codegen );
    rt->codegen.userdata = rt;
    rt->codegen.report   = rt_cgen_report;
    rt->codegen.halt     = rt_cgen_halt;

    rt->userdata = 0;
    rt->report   = 0;
    rt->halt     = 0;
}
