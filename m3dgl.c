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


#include "m3dgl.h"
#include "m3dpriv.h"
#include "glload.h"

/*
The hardware (OpenGL 2) renderer of the mesh module (docs/3d-mesh-subsystem.md "Hardware renderer"): the same scene as the software renderer, drawn
by the graphics card into a framebuffer object and read back as 32 bit ARGB. It runs on the thread that calls m3d_render() (the program thread),
with a context of its own on a hidden window; the window host's screen thread and its SDL renderer are not involved.

What is the same as in the software renderer: the camera (a pinhole with square pixels, the horizon shifted by the pitch), the near plane (2), the
far limit, the depth test, the masked pictures (index 0 see-through), the order of the passes (sky, opaque objects, things back to front, translucent
objects back to front) and the light formula (the object's light + the lamps' light at the point, times the fog). What differs: the picture
indices are looked up in the palette per pixel and the light scales the color (the software renderer picks the nearest palette color of 32 light
levels), translucent surfaces mix half and half in true color, the sky is looked up per pixel as well.
*/

#define NKEYS       ( M3D_MAXTEX + 256 )        // picture keys: 0 .. M3D_MAXTEX-1 a picture, M3D_MAXTEX + i a flat color (palette index i)
#define LIGHTK      ( 31.0 / 255.0 )            // object light (0..255) -> light level (0..31)
#define VFLOATS     6                           // x y z (world) u v light

typedef struct { int key; float v[3 * VFLOATS]; } gtri_t;

typedef struct {
    GLint eye, r, up, fw, proj, tex, pal, fog, far_, masked, alpha, uselamps, lampp, lampa;
    GLint apos, auv, alight;
} prog_t;

typedef struct {
    GLint yaw, f, cx, horizon, rep, shift, size, tex, pal;
    GLint apos;
} skyprog_t;

typedef struct {
    SDL_Window*   win;
    SDL_GLContext ctx;
    glapi_t       gl;
    GLuint        prog, skyprog;
    prog_t        pu;
    skyprog_t     su;
    GLuint        fbo, colortex, depthrb;
    GLuint        paltex, idtex, vbo, skyvbo;
    int           fw, fh;                       // the size of the framebuffer object (0: none yet)
    bool          paldirty;
    GLuint        texid[M3D_MAXTEX];            // 0: not uploaded
    unsigned      texgen[M3D_MAXTEX];
    gtri_t*       tris;  int ntris, captris;
    float*        vb;    size_t capvb;
    uint8_t*      rgba;                         // the read back picture
    uint32_t*     out;                          // ... as ARGB (what m3d_render32() returns)
    int           ow, oh;
} gl_t;

// -- shaders --------------------------------------------------------------------------------------------------------------------

static const char* const vs_obj =
    "#version 120\n"
    "uniform vec3 u_eye, u_r, u_up, u_fw;\n"
    "uniform vec4 u_proj;\n"
    "attribute vec3 a_pos; attribute vec2 a_uv; attribute float a_light;\n"
    "varying vec2 v_uv; varying float v_light; varying vec3 v_world; varying float v_z;\n"
    "void main() {\n"
    "    vec3 d = a_pos - u_eye;\n"
    "    float vx = dot( d, u_r ), vy = dot( d, u_up ), vz = dot( d, u_fw );\n"
    "    gl_Position = vec4( vx * u_proj.x, vy * u_proj.y, vz * u_proj.z + u_proj.w, vz );\n"
    "    v_uv = a_uv; v_light = a_light; v_world = a_pos; v_z = vz;\n"
    "}\n";

static const char* const fs_obj =
    "#version 120\n"
    "uniform sampler2D u_tex, u_pal;\n"
    "uniform float u_fog, u_far, u_masked, u_alpha, u_uselamps;\n"
    "uniform vec4 u_lampp[8];\n"
    "uniform float u_lampa[8];\n"
    "varying vec2 v_uv; varying float v_light; varying vec3 v_world; varying float v_z;\n"
    "void main() {\n"
    "    float i = floor( texture2D( u_tex, v_uv ).r * 255.0 + 0.5 );\n"
    "    if ( u_masked > 0.5 && i < 0.5 ) discard;\n"
    "    if ( u_far > 0.0 && v_z > u_far ) discard;\n"
    "    float lv = v_light;\n"
    "    if ( u_uselamps > 0.5 ) {\n"
    "        for ( int k = 0; k < 8; ++k ) {\n"
    "            if ( u_lampp[k].w > 0.0 && u_lampa[k] > 0.0 ) {\n"
    "                float dd = distance( v_world, u_lampp[k].xyz );\n"
    "                if ( dd < u_lampp[k].w ) lv += u_lampa[k] * ( 1.0 - dd / u_lampp[k].w ) * 0.12156863;\n"
    "            }\n"
    "        }\n"
    "    }\n"
    "    if ( u_fog > 0.0 ) lv *= max( 1.0 - v_z / u_fog, 0.0 );\n"
    "    float b = clamp( lv, 0.0, 31.0 ) / 31.0;\n"
    "    gl_FragColor = vec4( texture2D( u_pal, vec2( ( i + 0.5 ) / 256.0, 0.5 ) ).rgb * b, u_alpha );\n"
    "}\n";

static const char* const vs_sky =
    "#version 120\n"
    "attribute vec2 a_pos;\n"
    "void main() { gl_Position = vec4( a_pos, 0.0, 1.0 ); }\n";

static const char* const fs_sky =
    "#version 120\n"
    "uniform sampler2D u_tex, u_pal;\n"
    "uniform float u_yaw, u_f, u_cx, u_horizon, u_rep, u_shift;\n"
    "uniform vec2 u_size;\n"
    "void main() {\n"
    "    float ang = ( u_yaw - degrees( atan( ( gl_FragCoord.x - u_cx ) / u_f ) ) ) / 360.0;\n"
    "    ang -= floor( ang );\n"
    "    float tu = floor( mod( ang * u_size.x * u_rep, u_size.x ) );\n"
    "    float r = clamp( float( int( u_horizon - gl_FragCoord.y + u_shift ) ), 0.0, u_size.y - 1.0 );\n"
    "    float row = u_size.y - 1.0 - r;\n"
    "    float i = floor( texture2D( u_tex, vec2( ( tu + 0.5 ) / u_size.x, ( row + 0.5 ) / u_size.y ) ).r * 255.0 + 0.5 );\n"
    "    gl_FragColor = vec4( texture2D( u_pal, vec2( ( i + 0.5 ) / 256.0, 0.5 ) ).rgb, 1.0 );\n"
    "}\n";

static GLuint make_shader( gl_t* g, GLenum type, const char* src ) {
    glapi_t* gl = &g->gl;
    GLuint s = gl->CreateShader( type );
    gl->ShaderSource( s, 1, &src, 0 );
    gl->CompileShader( s );
    GLint ok = 0;
    gl->GetShaderiv( s, GL_COMPILE_STATUS, &ok );
    if ( !ok ) {
        char log[600];
        log[0] = 0;
        gl->GetShaderInfoLog( s, sizeof(log), 0, log );
        fprintf( stderr, "M3D hardware renderer: shader: %s\n", log );
        gl->DeleteShader( s );
        return 0;
    }
    return s;
}

static GLuint make_program( gl_t* g, const char* vs, const char* fs ) {
    glapi_t* gl = &g->gl;
    GLuint v = make_shader( g, GL_VERTEX_SHADER, vs ), f = make_shader( g, GL_FRAGMENT_SHADER, fs );
    if ( v == 0 || f == 0 ) {
        if ( v != 0 ) { gl->DeleteShader( v ); }
        if ( f != 0 ) { gl->DeleteShader( f ); }
        return 0;
    }
    GLuint p = gl->CreateProgram();
    gl->AttachShader( p, v );
    gl->AttachShader( p, f );
    gl->LinkProgram( p );
    gl->DeleteShader( v );
    gl->DeleteShader( f );
    GLint ok = 0;
    gl->GetProgramiv( p, GL_LINK_STATUS, &ok );
    if ( !ok ) {
        char log[600];
        log[0] = 0;
        gl->GetProgramInfoLog( p, sizeof(log), 0, log );
        fprintf( stderr, "M3D hardware renderer: link: %s\n", log );
        gl->DeleteProgram( p );
        return 0;
    }
    return p;
}

// -- creation ---------------------------------------------------------------------------------------------------------------------

static void gl_make_current( gl_t* g ) {
    if ( SDL_GL_GetCurrentContext() != g->ctx ) {
        SDL_GL_MakeCurrent( g->win, g->ctx );
    }
}

static void delete_fbo( gl_t* g ) {
    glapi_t* gl = &g->gl;
    if ( g->fbo != 0 ) { gl->DeleteFramebuffers( 1, &g->fbo ); g->fbo = 0; }
    if ( g->colortex != 0 ) { gl->DeleteTextures( 1, &g->colortex ); g->colortex = 0; }
    if ( g->depthrb != 0 ) { gl->DeleteRenderbuffers( 1, &g->depthrb ); g->depthrb = 0; }
    g->fw = g->fh = 0;
}

static bool make_fbo( gl_t* g, int w, int h ) {
    glapi_t* gl = &g->gl;
    delete_fbo( g );
    gl->GenTextures( 1, &g->colortex );
    gl->BindTexture( GL_TEXTURE_2D, g->colortex );
    gl->TexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    gl->GenRenderbuffers( 1, &g->depthrb );
    gl->BindRenderbuffer( GL_RENDERBUFFER, g->depthrb );
    gl->RenderbufferStorage( GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h );
    gl->GenFramebuffers( 1, &g->fbo );
    gl->BindFramebuffer( GL_FRAMEBUFFER, g->fbo );
    gl->FramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g->colortex, 0 );
    gl->FramebufferRenderbuffer( GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, g->depthrb );
    if ( gl->CheckFramebufferStatus( GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
        delete_fbo( g );
        return false;
    }
    g->fw = w; g->fh = h;
    return true;
}

static void gl_destroy( m3d_t* m, void* rd );

static void* gl_create( m3d_t* m ) {
    (void) m;
    if ( glload_state() != GLLOAD_YES ) {
        return 0;
    }
    gl_t* g = (gl_t*) calloc( 1, sizeof(gl_t) );
    if ( g == 0 ) {
        return 0;
    }
    g->win = SDL_CreateWindow( "", 0, 0, 16, 16, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN );
    if ( g->win == 0 ) {
        free( g );
        return 0;
    }
    g->ctx = SDL_GL_CreateContext( g->win );
    if ( g->ctx == 0 ) {
        SDL_DestroyWindow( g->win );
        free( g );
        return 0;
    }
    SDL_GL_MakeCurrent( g->win, g->ctx );
    char err[100];
    if ( !glload_bind( &g->gl, err, sizeof(err) ) ) {
        gl_destroy( m, g );
        return 0;
    }
    glapi_t* gl = &g->gl;
    g->prog = make_program( g, vs_obj, fs_obj );
    g->skyprog = make_program( g, vs_sky, fs_sky );
    if ( g->prog == 0 || g->skyprog == 0 ) {
        gl_destroy( m, g );
        return 0;
    }
    prog_t* p = &g->pu;
    p->eye = gl->GetUniformLocation( g->prog, "u_eye" );
    p->r = gl->GetUniformLocation( g->prog, "u_r" );
    p->up = gl->GetUniformLocation( g->prog, "u_up" );
    p->fw = gl->GetUniformLocation( g->prog, "u_fw" );
    p->proj = gl->GetUniformLocation( g->prog, "u_proj" );
    p->tex = gl->GetUniformLocation( g->prog, "u_tex" );
    p->pal = gl->GetUniformLocation( g->prog, "u_pal" );
    p->fog = gl->GetUniformLocation( g->prog, "u_fog" );
    p->far_ = gl->GetUniformLocation( g->prog, "u_far" );
    p->masked = gl->GetUniformLocation( g->prog, "u_masked" );
    p->alpha = gl->GetUniformLocation( g->prog, "u_alpha" );
    p->uselamps = gl->GetUniformLocation( g->prog, "u_uselamps" );
    p->lampp = gl->GetUniformLocation( g->prog, "u_lampp" );
    p->lampa = gl->GetUniformLocation( g->prog, "u_lampa" );
    p->apos = gl->GetAttribLocation( g->prog, "a_pos" );
    p->auv = gl->GetAttribLocation( g->prog, "a_uv" );
    p->alight = gl->GetAttribLocation( g->prog, "a_light" );
    skyprog_t* s = &g->su;
    s->yaw = gl->GetUniformLocation( g->skyprog, "u_yaw" );
    s->f = gl->GetUniformLocation( g->skyprog, "u_f" );
    s->cx = gl->GetUniformLocation( g->skyprog, "u_cx" );
    s->horizon = gl->GetUniformLocation( g->skyprog, "u_horizon" );
    s->rep = gl->GetUniformLocation( g->skyprog, "u_rep" );
    s->shift = gl->GetUniformLocation( g->skyprog, "u_shift" );
    s->size = gl->GetUniformLocation( g->skyprog, "u_size" );
    s->tex = gl->GetUniformLocation( g->skyprog, "u_tex" );
    s->pal = gl->GetUniformLocation( g->skyprog, "u_pal" );
    s->apos = gl->GetAttribLocation( g->skyprog, "a_pos" );
    if ( p->apos < 0 || p->auv < 0 || p->alight < 0 || s->apos < 0 ) {
        gl_destroy( m, g );
        return 0;
    }
    // the palette (256 x 1) and the identity picture (palette index = column) for flat colors
    gl->PixelStorei( GL_UNPACK_ALIGNMENT, 1 );
    gl->GenTextures( 1, &g->paltex );
    gl->BindTexture( GL_TEXTURE_2D, g->paltex );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    gl->TexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );
    g->paldirty = true;
    uint8_t ident[256];
    for ( int i=0; i < 256; ++i ) { ident[i] = (uint8_t) i; }
    gl->GenTextures( 1, &g->idtex );
    gl->BindTexture( GL_TEXTURE_2D, g->idtex );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE );
    gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE );
    gl->TexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE, 256, 1, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, ident );
    gl->GenBuffers( 1, &g->vbo );
    gl->GenBuffers( 1, &g->skyvbo );
    static const GLfloat tri[6] = { -1, -1, 3, -1, -1, 3 };
    gl->BindBuffer( GL_ARRAY_BUFFER, g->skyvbo );
    gl->BufferData( GL_ARRAY_BUFFER, sizeof(tri), tri, GL_STATIC_DRAW );
    gl->BindBuffer( GL_ARRAY_BUFFER, 0 );
    if ( gl->GetError() != GL_NO_ERROR ) {
        gl_destroy( m, g );
        return 0;
    }
    return g;
}

static void gl_destroy( m3d_t* m, void* rd ) {
    (void) m;
    gl_t* g = (gl_t*) rd;
    if ( g == 0 ) {
        return;
    }
    if ( g->ctx != 0 && g->win != 0 ) {
        gl_make_current( g );
        glapi_t* gl = &g->gl;
        if ( gl->DeleteTextures != 0 ) {
            delete_fbo( g );
            for ( int i=0; i < M3D_MAXTEX; ++i ) {
                if ( g->texid[i] != 0 ) { gl->DeleteTextures( 1, &g->texid[i] ); }
            }
            if ( g->paltex != 0 ) { gl->DeleteTextures( 1, &g->paltex ); }
            if ( g->idtex != 0 ) { gl->DeleteTextures( 1, &g->idtex ); }
            if ( g->vbo != 0 ) { gl->DeleteBuffers( 1, &g->vbo ); }
            if ( g->skyvbo != 0 ) { gl->DeleteBuffers( 1, &g->skyvbo ); }
            if ( g->prog != 0 ) { gl->DeleteProgram( g->prog ); }
            if ( g->skyprog != 0 ) { gl->DeleteProgram( g->skyprog ); }
        }
        SDL_GL_MakeCurrent( g->win, 0 );
    }
    if ( g->ctx != 0 ) { SDL_GL_DeleteContext( g->ctx ); }
    if ( g->win != 0 ) { SDL_DestroyWindow( g->win ); }
    free( g->tris ); free( g->vb ); free( g->rgba ); free( g->out );
    free( g );
}

static bool gl_resize( m3d_t* m, void* rd, int bw, int bh ) {
    (void) m;
    gl_t* g = (gl_t*) rd;
    uint8_t* rgba = (uint8_t*) malloc( (size_t) bw * (size_t) bh * 4U );
    uint32_t* out = (uint32_t*) calloc( (size_t) bw * (size_t) bh, sizeof(uint32_t) );
    if ( rgba == 0 || out == 0 ) {
        free( rgba ); free( out );
        return false;
    }
    free( g->rgba ); free( g->out );
    g->rgba = rgba; g->out = out; g->ow = bw; g->oh = bh;       // (the framebuffer object follows at the next frame)
    return true;
}

static void gl_palette( m3d_t* m, void* rd ) {
    (void) m;
    ((gl_t*) rd)->paldirty = true;
}

// -- pictures ---------------------------------------------------------------------------------------------------------------------

static void bind_texture( gl_t* g, const m3d_t* m, int key ) {
    glapi_t* gl = &g->gl;
    if ( key >= M3D_MAXTEX ) {
        gl->BindTexture( GL_TEXTURE_2D, g->idtex );
        return;
    }
    const tex_t* tx = tex_get( m, key );
    if ( tx == 0 ) {
        gl->BindTexture( GL_TEXTURE_2D, g->idtex );
        return;
    }
    if ( g->texid[key] == 0 || g->texgen[key] != tx->gen ) {
        size_t n = (size_t) tx->w * (size_t) tx->h;
        uint8_t* img = (uint8_t*) malloc( n );
        if ( img == 0 ) {
            gl->BindTexture( GL_TEXTURE_2D, g->idtex );
            return;
        }
        for ( int v=0; v < tx->h; ++v ) {
            for ( int u=0; u < tx->w; ++u ) {
                img[ (size_t) v * (size_t) tx->w + (size_t) u ] = tx->col[ (size_t) u * (size_t) tx->h + (size_t) v ];
            }
        }
        if ( g->texid[key] == 0 ) {
            gl->GenTextures( 1, &g->texid[key] );
        }
        gl->BindTexture( GL_TEXTURE_2D, g->texid[key] );
        gl->PixelStorei( GL_UNPACK_ALIGNMENT, 1 );
        gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
        gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
        gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
        gl->TexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
        gl->TexImage2D( GL_TEXTURE_2D, 0, GL_LUMINANCE, tx->w, tx->h, 0, GL_LUMINANCE, GL_UNSIGNED_BYTE, img );
        g->texgen[key] = tx->gen;
        free( img );
        return;
    }
    gl->BindTexture( GL_TEXTURE_2D, g->texid[key] );
}

static void upload_palette( gl_t* g, const m3d_t* m ) {
    uint8_t px[256 * 4];
    for ( int i=0; i < 256; ++i ) {
        px[i*4] = (uint8_t)( m->pal[i] >> 16 ); px[i*4+1] = (uint8_t)( m->pal[i] >> 8 ); px[i*4+2] = (uint8_t) m->pal[i]; px[i*4+3] = 255;
    }
    glapi_t* gl = &g->gl;
    gl->BindTexture( GL_TEXTURE_2D, g->paltex );
    gl->PixelStorei( GL_UNPACK_ALIGNMENT, 1 );
    gl->TexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, 256, 1, GL_RGBA, GL_UNSIGNED_BYTE, px );
    g->paldirty = false;
}

// -- triangles --------------------------------------------------------------------------------------------------------------------

static int pic_key( const m3d_t* m, int tex ) {
    return tex_get( m, tex ) != 0 ? tex : M3D_MAXTEX + ( tex & 255 );
}

static gtri_t* add_tri( gl_t* g ) {
    if ( g->ntris >= g->captris ) {
        int nc = g->captris == 0 ? 1024 : g->captris * 2;
        gtri_t* nt = (gtri_t*) realloc( g->tris, sizeof(gtri_t) * (size_t) nc );
        if ( nt == 0 ) {
            return 0;
        }
        g->tris = nt; g->captris = nc;
    }
    return &g->tris[ g->ntris++ ];
}

static void put_vertex( float* d, v3_t p, double u, double v, double light ) {
    d[0] = (float) p.x; d[1] = (float) p.y; d[2] = (float) p.z; d[3] = (float) u; d[4] = (float) v; d[5] = (float) light;
}

static int cmp_key( const void* a, const void* b ) {
    int ka = ((const gtri_t*) a)->key, kb = ((const gtri_t*) b)->key;
    return ka < kb ? -1 : ka > kb;
}

typedef struct {
    double f, cx, cy;
    v3_t   eye, r, up, fw;
} cam_t;

// draws the collected triangles (sorted by picture) and empties the list; key = picture * 2 + masked
static void flush_tris( gl_t* g, const m3d_t* m ) {
    if ( g->ntris == 0 ) {
        return;
    }
    glapi_t* gl = &g->gl;
    qsort( g->tris, (size_t) g->ntris, sizeof(gtri_t), cmp_key );
    size_t need = (size_t) g->ntris * 3U * VFLOATS;
    if ( need > g->capvb ) {
        float* nv = (float*) realloc( g->vb, need * sizeof(float) );
        if ( nv == 0 ) {
            g->ntris = 0;
            return;
        }
        g->vb = nv; g->capvb = need;
    }
    for ( int i=0; i < g->ntris; ++i ) {
        memcpy( g->vb + (size_t) i * 3U * VFLOATS, g->tris[i].v, sizeof(float) * 3U * VFLOATS );
    }
    gl->BindBuffer( GL_ARRAY_BUFFER, g->vbo );
    gl->BufferData( GL_ARRAY_BUFFER, (GLsizeiptr)( need * sizeof(float) ), g->vb, GL_STREAM_DRAW );
    const prog_t* p = &g->pu;
    const GLsizei stride = (GLsizei)( VFLOATS * sizeof(float) );
    gl->EnableVertexAttribArray( (GLuint) p->apos );
    gl->EnableVertexAttribArray( (GLuint) p->auv );
    gl->EnableVertexAttribArray( (GLuint) p->alight );
    gl->VertexAttribPointer( (GLuint) p->apos, 3, GL_FLOAT, GL_FALSE, stride, (const void*) 0 );
    gl->VertexAttribPointer( (GLuint) p->auv, 2, GL_FLOAT, GL_FALSE, stride, (const void*)( 3 * sizeof(float) ) );
    gl->VertexAttribPointer( (GLuint) p->alight, 1, GL_FLOAT, GL_FALSE, stride, (const void*)( 5 * sizeof(float) ) );
    int i = 0;
    while ( i < g->ntris ) {
        int key = g->tris[i].key, j = i;
        while ( j < g->ntris && g->tris[j].key == key ) {
            ++j;
        }
        bind_texture( g, m, key >> 1 );
        gl->Uniform1f( p->masked, ( key & 1 ) != 0 ? 1.0f : 0.0f );
        gl->DrawArrays( GL_TRIANGLES, i * 3, ( j - i ) * 3 );
        i = j;
    }
    gl->DisableVertexAttribArray( (GLuint) p->apos );
    gl->DisableVertexAttribArray( (GLuint) p->auv );
    gl->DisableVertexAttribArray( (GLuint) p->alight );
    gl->BindBuffer( GL_ARRAY_BUFFER, 0 );
    g->ntris = 0;
}

static bool object_seen( const m3d_t* m, const cam_t* c, const obj_t* o, double* dist ) {
    v3_t d = { o->cx - c->eye.x, o->cy - c->eye.y, o->cz - c->eye.z };
    double zv = vdot( d, c->fw ), xv = vdot( d, c->r ), yv = vdot( d, c->up );
    *dist = zv;
    if ( zv + o->rad < NEARZ ) {
        return false;
    }
    if ( m->far > 0 && zv - o->rad > m->far ) {
        return false;
    }
    double zz = zv + o->rad;
    if ( zz > 0 && fabs( xv ) > zz * ( c->cx / c->f ) + o->rad * 1.5 ) {
        return false;
    }
    if ( zz > 0 && fabs( yv ) > zz * ( c->cy / c->f ) + o->rad * 1.5 ) {
        return false;
    }
    return true;
}

// the visible triangles of an object into the list
static void collect_object( gl_t* g, m3d_t* m, const cam_t* c, const obj_t* o ) {
    bool twosided = ( o->flags & M3D_OF_TWOSIDED ) != 0;
    int masked = ( o->flags & M3D_OF_MASKED ) != 0 || ( o->flags & M3D_OF_TRANS ) != 0 ? 1 : 0;
    double light = (double) o->light * LIGHTK;
    for ( int j=0; j < o->nt; ++j ) {
        const mt_t* t = &o->t[j];
        v3_t wa = o->wv[t->a], wb = o->wv[t->b], wc = o->wv[t->c];
        if ( !twosided && vdot( vcross( vsub( wb, wa ), vsub( wc, wa ) ), vsub( c->eye, wa ) ) <= 0 ) {
            continue;
        }
        gtri_t* gt = add_tri( g );
        if ( gt == 0 ) {
            return;
        }
        int pk = pic_key( m, t->tex );
        gt->key = pk * 2 + masked;
        const tex_t* tx = tex_get( m, t->tex );
        const v3_t wp[3] = { wa, wb, wc };
        for ( int k=0; k < 3; ++k ) {
            double u, v;
            if ( tx != 0 ) {
                u = t->u[k] / (double) tx->w; v = t->v[k] / (double) tx->h;
            } else {
                u = ( (double)( t->tex & 255 ) + 0.5 ) / 256.0; v = 0.5;
            }
            put_vertex( &gt->v[k * VFLOATS], wp[k], u, v, light );
        }
        ++m->drawn;
    }
}

static void sort_desc( int* idx, const double* key, int n ) {
    for ( int gap = n / 2; gap > 0; gap /= 2 ) {
        for ( int i = gap; i < n; ++i ) {
            int t = idx[i];
            int j = i;
            while ( j >= gap && key[idx[j-gap]] < key[t] ) {
                idx[j] = idx[j-gap];
                j -= gap;
            }
            idx[j] = t;
        }
    }
}

static void set_pass( gl_t* g, bool depthwrite, bool blend, float alpha ) {
    glapi_t* gl = &g->gl;
    gl->DepthMask( depthwrite ? GL_TRUE : GL_FALSE );
    if ( blend ) {
        gl->Enable( GL_BLEND );
        gl->BlendFunc( GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA );
    } else {
        gl->Disable( GL_BLEND );
    }
    gl->Uniform1f( g->pu.alpha, alpha );
}

// -- the frame --------------------------------------------------------------------------------------------------------------------

static void draw_things_gl( gl_t* g, m3d_t* m, const cam_t* c ) {
    int n = 0;
    for ( int i=0; i < M3D_MAXTHINGS; ++i ) {
        const thing_t* t = &m->things[i];
        if ( !t->on ) {
            continue;
        }
        v3_t d = { t->x - c->eye.x, t->y - c->eye.y, t->z + t->h * 0.5 - c->eye.z };
        double zv = vdot( d, c->fw );
        if ( zv < NEARZ ) {
            continue;
        }
        m->order[n] = i; m->odist[i] = zv; ++n;
    }
    sort_desc( m->order, m->odist, n );
    for ( int k=0; k < n; ++k ) {
        const thing_t* t = &m->things[m->order[k]];
        double zv = m->odist[m->order[k]];
        v3_t pb = { t->x - c->eye.x, t->y - c->eye.y, t->z - c->eye.z };
        double xv = vdot( pb, c->r ), ybot = vdot( pb, c->up ), ytop = ybot + vdot( (v3_t){ 0, 0, t->h }, c->up );
        if ( t->w <= 0 || t->h <= 0 ) {
            continue;
        }
        // a rectangle facing the viewer at the depth of the thing's middle: corners in the view plane, back in world coordinates
        double xl = xv - t->w * 0.5, xr = xv + t->w * 0.5;
        v3_t base = vadd( c->eye, vmul( c->fw, zv ) );
        v3_t cn[4] = {
            vadd( base, vadd( vmul( c->r, xl ), vmul( c->up, ytop ) ) ),       // top left
            vadd( base, vadd( vmul( c->r, xr ), vmul( c->up, ytop ) ) ),       // top right
            vadd( base, vadd( vmul( c->r, xr ), vmul( c->up, ybot ) ) ),       // bottom right
            vadd( base, vadd( vmul( c->r, xl ), vmul( c->up, ybot ) ) )        // bottom left
        };
        const tex_t* tx = tex_get( m, t->tex );
        double uu[4] = { 0, 1, 1, 0 }, vv[4] = { 0, 0, 1, 1 };
        if ( tx == 0 ) {
            for ( int q=0; q < 4; ++q ) { uu[q] = ( (double)( t->tex & 255 ) + 0.5 ) / 256.0; vv[q] = 0.5; }
        }
        double light = (double) t->light * LIGHTK;
        static const int idx[6] = { 0, 1, 2, 0, 2, 3 };
        for ( int q=0; q < 2; ++q ) {
            gtri_t* gt = add_tri( g );
            if ( gt == 0 ) {
                return;
            }
            gt->key = pic_key( m, t->tex ) * 2 + 1;
            for ( int e=0; e < 3; ++e ) {
                int ci = idx[q * 3 + e];
                put_vertex( &gt->v[e * VFLOATS], cn[ci], uu[ci], vv[ci], light );
            }
        }
        set_pass( g, false, t->trans, 0.5f );
        flush_tris( g, m );
        ++m->drawn;
    }
}

static const void* gl_render( m3d_t* m, void* rd ) {
    gl_t* g = (gl_t*) rd;
    glapi_t* gl = &g->gl;
    if ( g->out == 0 ) {
        return 0;
    }
    gl_make_current( g );
    int W = m->bw, H = m->bh;
    if ( g->fw != W || g->fh != H ) {
        if ( !make_fbo( g, W, H ) ) {
            return 0;
        }
    }
    if ( g->paldirty ) {
        upload_palette( g, m );
    }
    cam_t c;
    double yr = m->yaw * DEG2RAD, pr = m->pitch * DEG2RAD;
    c.f = ( W * 0.5 ) / tan( m->fov * 0.5 * DEG2RAD );
    c.cx = W * 0.5; c.cy = H * 0.5;
    c.eye.x = m->x; c.eye.y = m->y; c.eye.z = eye_z( m );
    c.fw.x = cos( yr ) * cos( pr ); c.fw.y = sin( yr ) * cos( pr ); c.fw.z = sin( pr );
    c.r.x = sin( yr ); c.r.y = -cos( yr ); c.r.z = 0;
    c.up.x = -sin( pr ) * cos( yr ); c.up.y = -sin( pr ) * sin( yr ); c.up.z = cos( pr );
    double horizon = H * 0.5 + tan( pr ) * c.f;
    m->drawn = 0;

    gl->BindFramebuffer( GL_FRAMEBUFFER, g->fbo );
    gl->Viewport( 0, 0, W, H );
    gl->Disable( GL_CULL_FACE );
    gl->Disable( GL_BLEND );
    gl->DepthMask( GL_TRUE );
    gl->Disable( GL_DEPTH_TEST );
    // the sky: a picture looked up per pixel from the view direction, or one flat color
    const tex_t* sky = tex_get( m, m->skytex );
    float cr = 0, cg = 0, cb = 0;
    if ( sky == 0 && m->skytex >= 0 ) {
        uint32_t col = m->pal[ (uint8_t) m->skytex ];
        cr = (float)( ( col >> 16 ) & 255U ) / 255.0f; cg = (float)( ( col >> 8 ) & 255U ) / 255.0f; cb = (float)( col & 255U ) / 255.0f;
    }
    gl->ClearColor( cr, cg, cb, 1.0f );
    gl->Clear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
    if ( sky != 0 ) {
        const skyprog_t* s = &g->su;
        gl->UseProgram( g->skyprog );
        gl->ActiveTexture( GL_TEXTURE1 );
        gl->BindTexture( GL_TEXTURE_2D, g->paltex );
        gl->ActiveTexture( GL_TEXTURE0 );
        bind_texture( g, m, m->skytex );
        gl->Uniform1i( s->tex, 0 );
        gl->Uniform1i( s->pal, 1 );
        gl->Uniform1f( s->yaw, (GLfloat) m->yaw );
        gl->Uniform1f( s->f, (GLfloat) c.f );
        gl->Uniform1f( s->cx, (GLfloat) c.cx );
        gl->Uniform1f( s->horizon, (GLfloat) horizon );
        gl->Uniform1f( s->rep, (GLfloat) m->skyrep );
        gl->Uniform1f( s->shift, (GLfloat) m->skyshift );
        gl->Uniform2f( s->size, (GLfloat) sky->w, (GLfloat) sky->h );
        gl->BindBuffer( GL_ARRAY_BUFFER, g->skyvbo );
        gl->EnableVertexAttribArray( (GLuint) s->apos );
        gl->VertexAttribPointer( (GLuint) s->apos, 2, GL_FLOAT, GL_FALSE, 0, (const void*) 0 );
        gl->DrawArrays( GL_TRIANGLES, 0, 3 );
        gl->DisableVertexAttribArray( (GLuint) s->apos );
        gl->BindBuffer( GL_ARRAY_BUFFER, 0 );
    }

    // the world
    double zfar = m->far > 0 ? m->far * 1.01 + 10.0 : 40000.0;
    gl->UseProgram( g->prog );
    const prog_t* p = &g->pu;
    gl->ActiveTexture( GL_TEXTURE1 );
    gl->BindTexture( GL_TEXTURE_2D, g->paltex );
    gl->ActiveTexture( GL_TEXTURE0 );
    gl->Uniform1i( p->tex, 0 );
    gl->Uniform1i( p->pal, 1 );
    gl->Uniform3f( p->eye, (GLfloat) c.eye.x, (GLfloat) c.eye.y, (GLfloat) c.eye.z );
    gl->Uniform3f( p->r, (GLfloat) c.r.x, (GLfloat) c.r.y, (GLfloat) c.r.z );
    gl->Uniform3f( p->up, (GLfloat) c.up.x, (GLfloat) c.up.y, (GLfloat) c.up.z );
    gl->Uniform3f( p->fw, (GLfloat) c.fw.x, (GLfloat) c.fw.y, (GLfloat) c.fw.z );
    // the picture is drawn upside down (so that the first row read back is the top one): y is negated
    gl->Uniform4f( p->proj, (GLfloat)( c.f / c.cx ), (GLfloat)( -c.f / c.cy ), (GLfloat)( ( zfar + NEARZ ) / ( zfar - NEARZ ) ),
        (GLfloat)( -2.0 * NEARZ * zfar / ( zfar - NEARZ ) ) );
    gl->Uniform1f( p->fog, (GLfloat) m->fog );
    gl->Uniform1f( p->far_, (GLfloat) m->far );
    GLfloat lp[8 * 4], la[8];
    for ( int k=0; k < M3D_MAXLAMPS; ++k ) {
        const lamp_t* l = &m->lamps[k];
        bool on = l->r > 0 && l->amount > 0;
        lp[k*4] = (GLfloat) l->x; lp[k*4+1] = (GLfloat) l->y; lp[k*4+2] = (GLfloat) l->z; lp[k*4+3] = on ? (GLfloat) l->r : 0.0f;
        la[k] = on ? (GLfloat) l->amount : 0.0f;
    }
    gl->Uniform4fv( p->lampp, 8, lp );
    gl->Uniform1fv( p->lampa, 8, la );
    gl->Enable( GL_DEPTH_TEST );
    gl->DepthFunc( GL_LESS );

    // opaque objects; the translucent ones are kept for the end
    g->ntris = 0;
    gl->Uniform1f( p->uselamps, 1.0f );
    set_pass( g, true, false, 1.0f );
    int nt = 0;
    for ( int i=0; i < m->top; ++i ) {
        obj_t* o = &m->objs[i];
        if ( !o->used || !o->shown || o->nt == 0 ) {
            continue;
        }
        m3d_obj_update( o );
        double dist;
        if ( o->dirty || !object_seen( m, &c, o, &dist ) ) {
            continue;
        }
        if ( ( o->flags & M3D_OF_TRANS ) != 0 ) {
            m->order[nt] = i; m->odist[i] = dist; ++nt;
            continue;
        }
        collect_object( g, m, &c, o );
    }
    flush_tris( g, m );
    gl->Uniform1f( p->uselamps, 0.0f );
    draw_things_gl( g, m, &c );
    if ( nt > 0 ) {
        gl->Uniform1f( p->uselamps, 1.0f );
        sort_desc( m->order, m->odist, nt );
        for ( int k=0; k < nt; ++k ) {
            collect_object( g, m, &c, &m->objs[m->order[k]] );
            set_pass( g, false, true, 0.5f );
            flush_tris( g, m );
        }
    }
    gl->UseProgram( 0 );
    gl->DepthMask( GL_TRUE );
    gl->Disable( GL_BLEND );
    gl->Disable( GL_DEPTH_TEST );

    gl->PixelStorei( GL_PACK_ALIGNMENT, 1 );
    gl->ReadPixels( 0, 0, W, H, GL_RGBA, GL_UNSIGNED_BYTE, g->rgba );
    gl->BindFramebuffer( GL_FRAMEBUFFER, 0 );
    size_t np = (size_t) W * (size_t) H;
    for ( size_t i=0; i < np; ++i ) {
        const uint8_t* q = &g->rgba[i * 4U];
        g->out[i] = UINT32_C(0xFF000000) | (uint32_t) q[0] << 16 | (uint32_t) q[1] << 8 | (uint32_t) q[2];
    }
    return g->out;
}

static const m3drend_t gl_renderer = { "hardware", true, gl_create, gl_destroy, gl_resize, gl_palette, gl_render };

const struct _m3drend_t* m3d_gl_renderer( void ) {
    if ( glload_state() != GLLOAD_YES ) {
        return 0;
    }
    return &gl_renderer;
}
