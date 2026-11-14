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


#ifndef GLLOAD_H
#define GLLOAD_H   1

#ifndef STDTYPES_H
#include "stdtypes.h"
#endif

/*
Binding of OpenGL 2 without linking a GL library (docs/3d-mesh-subsystem.md "Hardware renderer"): SDL opens the system's GL library
(SDL_GL_LoadLibrary) and hands out the function addresses (SDL_GL_GetProcAddress); this module keeps the ones the renderer needs in a table
`glapi_t`. Only the SDL headers are used. Everything here belongs to the thread that owns SDL's video system (the screen thread), except
glload_state() / glload_info(), which any thread may call.

glload_probe() finds out whether OpenGL 2 can be used at all: it makes a hidden window with a context, binds the functions, reads the version
and tears everything down again, leaving the current context as it was (the SDL renderer's). glload_bind() fills a table while a context is
current. The framebuffer object functions come from the core names (GL 3.0), the ARB names or the EXT names, whichever the driver has.
*/

#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>

typedef struct _glapi_t {
    void (*Viewport)( GLint,GLint,GLsizei,GLsizei );
    void (*Clear)( GLbitfield );
    void (*ClearColor)( GLclampf,GLclampf,GLclampf,GLclampf );
    void (*Enable)( GLenum );
    void (*Disable)( GLenum );
    void (*DepthFunc)( GLenum );
    void (*DepthMask)( GLboolean );
    void (*BlendFunc)( GLenum,GLenum );
    void (*CullFace)( GLenum );
    void (*FrontFace)( GLenum );
    GLenum (*GetError)( void );
    const GLubyte* (*GetString)( GLenum );
    void (*GetIntegerv)( GLenum,GLint* );
    void (*ReadPixels)( GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,GLvoid* );
    void (*PixelStorei)( GLenum,GLint );
    void (*GenTextures)( GLsizei,GLuint* );
    void (*DeleteTextures)( GLsizei,const GLuint* );
    void (*BindTexture)( GLenum,GLuint );
    void (*TexImage2D)( GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid* );
    void (*TexSubImage2D)( GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const GLvoid* );
    void (*TexParameteri)( GLenum,GLenum,GLint );
    void (*DrawArrays)( GLenum,GLint,GLsizei );
    void (*DrawElements)( GLenum,GLsizei,GLenum,const GLvoid* );
    void (*Finish)( void );
    void (*ActiveTexture)( GLenum );
    void (*GenBuffers)( GLsizei,GLuint* );
    void (*DeleteBuffers)( GLsizei,const GLuint* );
    void (*BindBuffer)( GLenum,GLuint );
    void (*BufferData)( GLenum,GLsizeiptr,const GLvoid*,GLenum );
    void (*BufferSubData)( GLenum,GLintptr,GLsizeiptr,const GLvoid* );
    GLuint (*CreateShader)( GLenum );
    void (*ShaderSource)( GLuint,GLsizei,const GLchar* const*,const GLint* );
    void (*CompileShader)( GLuint );
    void (*GetShaderiv)( GLuint,GLenum,GLint* );
    void (*GetShaderInfoLog)( GLuint,GLsizei,GLsizei*,GLchar* );
    void (*DeleteShader)( GLuint );
    GLuint (*CreateProgram)( void );
    void (*AttachShader)( GLuint,GLuint );
    void (*LinkProgram)( GLuint );
    void (*GetProgramiv)( GLuint,GLenum,GLint* );
    void (*GetProgramInfoLog)( GLuint,GLsizei,GLsizei*,GLchar* );
    void (*UseProgram)( GLuint );
    void (*DeleteProgram)( GLuint );
    GLint (*GetUniformLocation)( GLuint,const GLchar* );
    GLint (*GetAttribLocation)( GLuint,const GLchar* );
    void (*Uniform1i)( GLint,GLint );
    void (*Uniform1f)( GLint,GLfloat );
    void (*Uniform2f)( GLint,GLfloat,GLfloat );
    void (*Uniform3f)( GLint,GLfloat,GLfloat,GLfloat );
    void (*Uniform4f)( GLint,GLfloat,GLfloat,GLfloat,GLfloat );
    void (*Uniform1fv)( GLint,GLsizei,const GLfloat* );
    void (*Uniform4fv)( GLint,GLsizei,const GLfloat* );
    void (*UniformMatrix4fv)( GLint,GLsizei,GLboolean,const GLfloat* );
    void (*EnableVertexAttribArray)( GLuint );
    void (*DisableVertexAttribArray)( GLuint );
    void (*VertexAttribPointer)( GLuint,GLint,GLenum,GLboolean,GLsizei,const GLvoid* );
    void (*GenFramebuffers)( GLsizei,GLuint* );
    void (*DeleteFramebuffers)( GLsizei,const GLuint* );
    void (*BindFramebuffer)( GLenum,GLuint );
    void (*FramebufferTexture2D)( GLenum,GLenum,GLenum,GLuint,GLint );
    GLenum (*CheckFramebufferStatus)( GLenum );
    void (*GenRenderbuffers)( GLsizei,GLuint* );
    void (*DeleteRenderbuffers)( GLsizei,const GLuint* );
    void (*BindRenderbuffer)( GLenum,GLuint );
    void (*RenderbufferStorage)( GLenum,GLenum,GLsizei,GLsizei );
    void (*FramebufferRenderbuffer)( GLenum,GLenum,GLenum,GLuint );
} glapi_t;

#define GLLOAD_UNKNOWN  (-1)            // glload_probe() has not run
#define GLLOAD_NO       0               // no OpenGL 2 (the reason is in glload_info())
#define GLLOAD_YES      1

typedef struct _glinfo_t {
    int  major, minor;                  // from GL_VERSION
    char version[128];                  // GL_VERSION
    char renderer[128];                 // GL_RENDERER
    char why[160];                      // when GLLOAD_NO: what was missing
} glinfo_t;

bool glload_bind( glapi_t* api, char* err, size_t errsz );       // a context must be current; false: a function (or the version) is missing
int  glload_probe( void );                                       // once; returns GLLOAD_YES / GLLOAD_NO (SDL video must be initialized)
int  glload_state( void );                                       // GLLOAD_UNKNOWN until glload_probe() has run
void glload_info( glinfo_t* info );                              // a copy of what the probe found

#endif
