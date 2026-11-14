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

/*
Automated test of the OpenGL binding (glload.[ch]). It needs a display: without one (or without OpenGL 2) it says so and succeeds, unless
the environment variable GLTEST_REQUIRE is set. With a context it checks that the probe leaves the current context alone, that all functions
bind, and that they work: a shader draws into a framebuffer object and glReadPixels returns what was drawn. Prints one line per failure; the exit
status is the number of failures.
*/

static int failures = 0, cases = 0;

#define CHECK(cond) do { ++cases; if ( !(cond) ) { ++failures; printf( "FAIL line %d: %s\n", __LINE__, #cond ); } } while (0)

static int skip( const char* why ) {
    printf( "gltest1: skipped (%s)\n", why );
    const char* req = getenv( "GLTEST_REQUIRE" );
    return req != 0 && req[0] != 0 && req[0] != '0' ? 1 : 0;
}

static GLuint make_shader( glapi_t* gl, GLenum type, const char* src ) {
    GLuint s = gl->CreateShader( type );
    gl->ShaderSource( s, 1, &src, 0 );
    gl->CompileShader( s );
    GLint ok = 0;
    gl->GetShaderiv( s, GL_COMPILE_STATUS, &ok );
    if ( !ok ) {
        char log[512];
        gl->GetShaderInfoLog( s, sizeof(log), 0, log );
        printf( "shader: %s\n", log );
    }
    return ok ? s : 0;
}

int main( void ) {
    CHECK( glload_state() == GLLOAD_UNKNOWN );
    if ( SDL_Init( SDL_INIT_VIDEO ) != 0 ) {
        return skip( "no video" );
    }
    SDL_Window* win = SDL_CreateWindow( "", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN );
    SDL_GLContext ctx = win != 0 ? SDL_GL_CreateContext( win ) : 0;
    if ( ctx == 0 ) {
        SDL_Quit();
        return skip( "no OpenGL context" );
    }
    CHECK( SDL_GL_MakeCurrent( win, ctx ) == 0 );
    int st = glload_probe();
    CHECK( st == glload_state() );
    CHECK( glload_probe() == st );                              // once
    CHECK( SDL_GL_GetCurrentContext() == ctx );                 // the probe gave the context back
    CHECK( SDL_GL_GetCurrentWindow() == win );
    glinfo_t info;
    glload_info( &info );
    if ( st != GLLOAD_YES ) {
        printf( "gltest1: no OpenGL 2 here: %s\n", info.why );
        SDL_GL_DeleteContext( ctx ); SDL_DestroyWindow( win ); SDL_Quit();
        return skip( "no OpenGL 2" ) + failures;
    }
    printf( "gltest1: OpenGL %d.%d, %s\n", info.major, info.minor, info.renderer );
    CHECK( info.major >= 2 );
    CHECK( info.version[0] != 0 );

    glapi_t gl;
    char err[100] = "";
    CHECK( glload_bind( &gl, err, sizeof(err) ) );
    if ( gl.CreateShader == 0 ) {
        printf( "bind: %s\n", err );
        return failures + 1;
    }
    // a plain clear, read back from the window's own framebuffer
    gl.ClearColor( 0.25f, 0.5f, 0.75f, 1.0f );
    gl.Clear( GL_COLOR_BUFFER_BIT );
    uint8_t px[4] = { 0, 0, 0, 0 };
    gl.ReadPixels( 2, 2, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, px );
    CHECK( px[0] >= 62 && px[0] <= 65 && px[1] >= 126 && px[1] <= 129 && px[2] >= 190 && px[2] <= 193 );

    // a framebuffer object with a texture, a shader, a vertex buffer: draw a triangle that covers everything, read the texture back
    GLuint tex = 0, fbo = 0, vbo = 0;
    gl.GenTextures( 1, &tex );
    gl.BindTexture( GL_TEXTURE_2D, tex );
    gl.TexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 8, 4, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    gl.TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    gl.TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    gl.GenFramebuffers( 1, &fbo );
    gl.BindFramebuffer( GL_FRAMEBUFFER, fbo );
    gl.FramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0 );
    CHECK( gl.CheckFramebufferStatus( GL_FRAMEBUFFER ) == GL_FRAMEBUFFER_COMPLETE );
    gl.Viewport( 0, 0, 8, 4 );
    gl.ClearColor( 0, 0, 0, 1 );
    gl.Clear( GL_COLOR_BUFFER_BIT );
    GLuint vs = make_shader( &gl, GL_VERTEX_SHADER, "attribute vec2 pos; void main() { gl_Position = vec4( pos, 0.0, 1.0 ); }" );
    GLuint fs = make_shader( &gl, GL_FRAGMENT_SHADER, "uniform vec3 col; void main() { gl_FragColor = vec4( col, 1.0 ); }" );
    CHECK( vs != 0 && fs != 0 );
    GLuint prog = gl.CreateProgram();
    gl.AttachShader( prog, vs );
    gl.AttachShader( prog, fs );
    gl.LinkProgram( prog );
    GLint linked = 0;
    gl.GetProgramiv( prog, GL_LINK_STATUS, &linked );
    CHECK( linked );
    gl.UseProgram( prog );
    GLint ucol = gl.GetUniformLocation( prog, "col" ), apos = gl.GetAttribLocation( prog, "pos" );
    CHECK( ucol >= 0 && apos >= 0 );
    gl.Uniform3f( ucol, 1.0f, 0.0f, 0.5f );
    static const GLfloat tri[6] = { -1, -1, 3, -1, -1, 3 };
    gl.GenBuffers( 1, &vbo );
    gl.BindBuffer( GL_ARRAY_BUFFER, vbo );
    gl.BufferData( GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW );
    gl.EnableVertexAttribArray( (GLuint) apos );
    gl.VertexAttribPointer( (GLuint) apos, 2, GL_FLOAT, GL_FALSE, 0, 0 );
    gl.DrawArrays( GL_TRIANGLES, 0, 3 );
    gl.PixelStorei( GL_PACK_ALIGNMENT, 1 );
    uint8_t img[8 * 4 * 4];
    memset( img, 0, sizeof(img) );
    gl.ReadPixels( 0, 0, 8, 4, GL_RGBA, GL_UNSIGNED_BYTE, img );
    CHECK( gl.GetError() == GL_NO_ERROR );
    int good = 0;
    for ( int i=0; i < 32; ++i ) {
        if ( img[i*4] == 255 && img[i*4+1] == 0 && img[i*4+2] >= 127 && img[i*4+2] <= 128 && img[i*4+3] == 255 ) {
            ++good;
        }
    }
    CHECK( good == 32 );
    gl.DisableVertexAttribArray( (GLuint) apos );
    gl.BindBuffer( GL_ARRAY_BUFFER, 0 );
    gl.UseProgram( 0 );
    gl.BindFramebuffer( GL_FRAMEBUFFER, 0 );
    gl.DeleteBuffers( 1, &vbo );
    gl.DeleteProgram( prog ); gl.DeleteShader( vs ); gl.DeleteShader( fs );
    gl.DeleteFramebuffers( 1, &fbo );
    gl.DeleteTextures( 1, &tex );
    CHECK( gl.GetError() == GL_NO_ERROR );

    SDL_GL_DeleteContext( ctx );
    SDL_DestroyWindow( win );
    SDL_Quit();
    printf( "gltest1: %d cases, %d failures\n", cases, failures );
    return failures;
}
