#pragma once
#include "a350_map.hpp"
#include "gl_state.hpp"
namespace oans {
// Submit A350's original interleaved triangles and B612 atlas coordinates.
// The caller provides the display transform and guards inherited GL state.
inline void draw_a350_vertices(const std::vector<A350Vertex>& vertices,double gain=1) {
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
}
