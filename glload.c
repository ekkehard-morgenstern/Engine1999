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


#include "glload.h"

static glinfo_t gl_info;
static int      gl_state = GLLOAD_UNKNOWN;

typedef struct { const char* name; const char* alt1; const char* alt2; size_t off; } glentry_t;

#define ENT(n)          { "gl" #n, 0, 0, offsetof( glapi_t, n ) }
#define ENTFB(n)        { "gl" #n, "gl" #n "ARB", "gl" #n "EXT", offsetof( glapi_t, n ) }

static const glentry_t gl_table[] = {
    ENT(Viewport),
    ENT(Clear),
    ENT(ClearColor),
    ENT(Enable),
    ENT(Disable),
    ENT(DepthFunc),
    ENT(DepthMask),
    ENT(BlendFunc),
    ENT(CullFace),
    ENT(FrontFace),
    ENT(GetError),
    ENT(GetString),
    ENT(GetIntegerv),
    ENT(ReadPixels),
    ENT(PixelStorei),
    ENT(GenTextures),
    ENT(DeleteTextures),
    ENT(BindTexture),
    ENT(TexImage2D),
    ENT(TexSubImage2D),
    ENT(TexParameteri),
    ENT(DrawArrays),
    ENT(DrawElements),
    ENT(Finish),
    ENT(ActiveTexture),
    ENT(GenBuffers),
    ENT(DeleteBuffers),
    ENT(BindBuffer),
    ENT(BufferData),
    ENT(BufferSubData),
    ENT(CreateShader),
    ENT(ShaderSource),
    ENT(CompileShader),
    ENT(GetShaderiv),
    ENT(GetShaderInfoLog),
    ENT(DeleteShader),
    ENT(CreateProgram),
    ENT(AttachShader),
    ENT(LinkProgram),
    ENT(GetProgramiv),
    ENT(GetProgramInfoLog),
    ENT(UseProgram),
    ENT(DeleteProgram),
    ENT(GetUniformLocation),
    ENT(GetAttribLocation),
    ENT(Uniform1i),
    ENT(Uniform1f),
    ENT(Uniform2f),
    ENT(Uniform3f),
    ENT(Uniform4f),
    ENT(UniformMatrix4fv),
    ENT(EnableVertexAttribArray),
    ENT(DisableVertexAttribArray),
    ENT(VertexAttribPointer),
    ENTFB(GenFramebuffers),
    ENTFB(DeleteFramebuffers),
    ENTFB(BindFramebuffer),
    ENTFB(FramebufferTexture2D),
    ENTFB(CheckFramebufferStatus),
    ENTFB(GenRenderbuffers),
    ENTFB(DeleteRenderbuffers),
    ENTFB(BindRenderbuffer),
    ENTFB(RenderbufferStorage),
    ENTFB(FramebufferRenderbuffer),
};

static void gl_set( char* err, size_t errsz, const char* fmt, const char* a ) {
    if ( err != 0 && errsz > 0 ) {
        snprintf( err, errsz, fmt, a );
    }
}

bool glload_bind( glapi_t* api, char* err, size_t errsz ) {
    memset( api, 0, sizeof(*api) );
    for ( size_t i=0; i < sizeof(gl_table) / sizeof(gl_table[0]); ++i ) {
        const glentry_t* e = &gl_table[i];
        void* p = SDL_GL_GetProcAddress( e->name );
        if ( p == 0 && e->alt1 != 0 ) {
            p = SDL_GL_GetProcAddress( e->alt1 );
        }
        if ( p == 0 && e->alt2 != 0 ) {
            p = SDL_GL_GetProcAddress( e->alt2 );
        }
        if ( p == 0 ) {
            gl_set( err, errsz, "%s is missing", e->name );
            memset( api, 0, sizeof(*api) );
            return false;
        }
        memcpy( (char*) api + e->off, &p, sizeof(p) );
    }
    return true;
}

static void gl_copy( char* dst, size_t n, const GLubyte* s ) {
    snprintf( dst, n, "%s", s != 0 ? (const char*) s : "" );
}

static int gl_fail( const char* why ) {
    snprintf( gl_info.why, sizeof(gl_info.why), "%s", why );
    return GLLOAD_NO;
}

static int gl_probe_run( void ) {
    memset( &gl_info, 0, sizeof(gl_info) );
    if ( SDL_GL_LoadLibrary( 0 ) != 0 ) {
        snprintf( gl_info.why, sizeof(gl_info.why), "no OpenGL library: %s", SDL_GetError() );
        return GLLOAD_NO;
    }
    SDL_Window* oldwin = SDL_GL_GetCurrentWindow();
    SDL_GLContext oldctx = SDL_GL_GetCurrentContext();
    int rv = GLLOAD_NO;
    SDL_Window* win = SDL_CreateWindow( "", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN );
    if ( win == 0 ) {
        snprintf( gl_info.why, sizeof(gl_info.why), "no OpenGL window: %s", SDL_GetError() );
        goto DONE;
    }
    SDL_GLContext ctx = SDL_GL_CreateContext( win );
    if ( ctx == 0 ) {
        snprintf( gl_info.why, sizeof(gl_info.why), "no OpenGL context: %s", SDL_GetError() );
        goto DONE_WIN;
    }
    glapi_t api;
    char err[96];
    if ( !glload_bind( &api, err, sizeof(err) ) ) {
        rv = gl_fail( err );
        goto DONE_CTX;
    }
    gl_copy( gl_info.version, sizeof(gl_info.version), api.GetString( GL_VERSION ) );
    gl_copy( gl_info.renderer, sizeof(gl_info.renderer), api.GetString( GL_RENDERER ) );
    if ( sscanf( gl_info.version, "%d.%d", &gl_info.major, &gl_info.minor ) != 2 ) {
        rv = gl_fail( "unreadable GL_VERSION" );
        goto DONE_CTX;
    }
    if ( gl_info.major < 2 ) {
        rv = gl_fail( "OpenGL older than 2.0" );
        goto DONE_CTX;
    }
    rv = GLLOAD_YES;
DONE_CTX:
    SDL_GL_DeleteContext( ctx );
DONE_WIN:
    SDL_DestroyWindow( win );
DONE:
    if ( oldwin != 0 && oldctx != 0 ) {
        SDL_GL_MakeCurrent( oldwin, oldctx );
    }
    return rv;
}

int glload_probe( void ) {
    if ( __atomic_load_n( &gl_state, __ATOMIC_ACQUIRE ) != GLLOAD_UNKNOWN ) {
        return gl_state;
    }
    const char* off = getenv( "ENGINE1999_NOGL" );
    int rv;
    if ( off != 0 && off[0] != 0 && off[0] != '0' ) {
        memset( &gl_info, 0, sizeof(gl_info) );
        snprintf( gl_info.why, sizeof(gl_info.why), "switched off by ENGINE1999_NOGL" );
        rv = GLLOAD_NO;
    } else {
        rv = gl_probe_run();
    }
    __atomic_store_n( &gl_state, rv, __ATOMIC_RELEASE );
    return rv;
}

int glload_state( void ) {
    return __atomic_load_n( &gl_state, __ATOMIC_ACQUIRE );
}

void glload_info( glinfo_t* info ) {
    if ( glload_state() == GLLOAD_UNKNOWN ) {
        memset( info, 0, sizeof(*info) );
        return;
    }
    *info = gl_info;
}
