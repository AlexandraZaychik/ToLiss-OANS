#pragma once
#include "a350_map.hpp"
#include "gl_state.hpp"
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>
#include <type_traits>
namespace oans {
namespace a350_gl_detail {
// Keep the original path for small batches and older compatibility contexts.
inline void immediate(const std::vector<A350Vertex>& vertices,double gain) {
    GLint env;glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&env);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glBegin(GL_TRIANGLES);
    for(const auto& v:vertices) {
        glColor4ub(GLubyte((v.rgba&255)*gain),GLubyte(((v.rgba>>8)&255)*gain),
                   GLubyte(((v.rgba>>16)&255)*gain),GLubyte(v.rgba>>24));
        glTexCoord2f(v.u,v.v);glVertex2f(v.x,v.y);
    }
    glEnd();glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,env);
}
inline bool batch(const std::vector<A350Vertex>& vertices,double gain) {
    if(vertices.empty())return true;
    if(!current_gl_context()||!std::isfinite(gain)||gain<0||gain>1||
       vertices.size()>static_cast<size_t>(std::numeric_limits<GLsizei>::max()))return false;
    // The supported Windows/macOS hosts are little endian: ABGR integers expose
    // RGBA bytes directly. Retain the immediate path on any other byte order.
    const std::uint32_t endian=1;
    if(*reinterpret_cast<const unsigned char*>(&endian)!=1)return false;
    static_assert(std::is_standard_layout<A350Vertex>::value,"GL vertex offsets require standard layout");
    using Bind=void(APIENTRY*)(GLenum,GLuint);
    using ClientTexture=void(APIENTRY*)(GLenum);
    using BindVao=void(APIENTRY*)(GLuint);
    auto bind_buffer=gl_proc<Bind>("glBindBuffer");
    auto client_texture=gl_proc<ClientTexture>("glClientActiveTexture");
    auto bind_vao=gl_proc<BindVao>("glBindVertexArray");
    if(!bind_buffer||!client_texture)return false;
    GLint depth=0,limit=0;glGetIntegerv(GL_CLIENT_ATTRIB_STACK_DEPTH,&depth);
    glGetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH,&limit);
    if(depth>=limit)return false;

    // Preserve the exact byte truncation used by glColor4ub, including alpha.
    // Only dimmed output needs scratch storage; capacity survives later draws.
    thread_local std::vector<std::uint32_t> colors;
    if(gain!=1) {
        try {colors.resize(vertices.size());}catch(const std::bad_alloc&){return false;}
        for(size_t i=0;i<vertices.size();++i) {
            const auto c=vertices[i].rgba;
            colors[i]=std::uint32_t(GLubyte((c&255)*gain))|
                (std::uint32_t(GLubyte(((c>>8)&255)*gain))<<8)|
                (std::uint32_t(GLubyte(((c>>16)&255)*gain))<<16)|(c&0xff000000u);
        }
    }
    GLint env=0,buffer=0,vao=0,client=0,units=1;
    glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&env);
    glGetIntegerv(0x8894,&buffer); // GL_ARRAY_BUFFER_BINDING
    glGetIntegerv(0x84E1,&client); // GL_CLIENT_ACTIVE_TEXTURE
    glGetIntegerv(0x84E2,&units); // GL_MAX_TEXTURE_UNITS (fixed function)
    if(bind_vao){glGetIntegerv(0x85B5,&vao);if(vao)bind_vao(0);}
    glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    bind_buffer(0x8892,0); // Client pointers must never be interpreted as VBO offsets.
    for(int i=1;i<units;++i){client_texture(0x84C0+i);glDisableClientState(GL_TEXTURE_COORD_ARRAY);}
    client_texture(0x84C0);
    glDisableClientState(GL_NORMAL_ARRAY);glDisableClientState(GL_INDEX_ARRAY);
    glDisableClientState(GL_EDGE_FLAG_ARRAY);
    glDisableClientState(0x845E);glDisableClientState(0x8457); // Secondary color / fog coordinate.
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    const auto* data=reinterpret_cast<const unsigned char*>(vertices.data());
    glVertexPointer(2,GL_FLOAT,sizeof(A350Vertex),data+offsetof(A350Vertex,x));
    glTexCoordPointer(2,GL_FLOAT,sizeof(A350Vertex),data+offsetof(A350Vertex,u));
    if(gain==1)glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(A350Vertex),data+offsetof(A350Vertex,rgba));
    else glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(std::uint32_t),colors.data());
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(vertices.size()));
    glPopClientAttrib();
    if(bind_vao&&vao)bind_vao(static_cast<GLuint>(vao));
    bind_buffer(0x8892,static_cast<GLuint>(buffer));client_texture(static_cast<GLenum>(client));
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,env);
    // Array draws leave current attributes unspecified. Match the immediate
    // path's final values for any subsequent fixed-function draw by the caller.
    const auto& last=vertices.back();
    glColor4ub(GLubyte((last.rgba&255)*gain),GLubyte(((last.rgba>>8)&255)*gain),
        GLubyte(((last.rgba>>16)&255)*gain),GLubyte(last.rgba>>24));
    glTexCoord2f(last.u,last.v);
    return true;
}
}
// Submit A350's existing triangles and atlas UVs. The caller owns the display
// transform and server-state guard; this function also restores client arrays.
inline constexpr size_t a350_batch_min_vertices=8192;
inline void draw_a350_vertices(const std::vector<A350Vertex>& vertices,double gain=1) {
    if(vertices.empty())return;
    if(vertices.size()>=a350_batch_min_vertices&&a350_gl_detail::batch(vertices,gain))return;
    a350_gl_detail::immediate(vertices,gain);
}
}
