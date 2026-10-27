#pragma once
#include "gl_platform.hpp"
#include <cmath>
#include <optional>

namespace oans {
struct TextureRect {double l,b,w,h;};
// The inspected aircraft uses ATTR_cockpit for the full texture, without panel
// regions. ToLiss enlarges that render target while legacy panel bounds can
// remain 1024. Recover model coordinates from the actual draw transform instead
// of treating legacy panel units as the texture size or guessing a resolution.
inline std::optional<TextureRect> texture_rectangle(bool reverse_y=false) {
    GLdouble model[16],projection[16],px,py,depth;
    GLint viewport[4];glGetDoublev(GL_MODELVIEW_MATRIX,model);glGetDoublev(GL_PROJECTION_MATRIX,projection);glGetIntegerv(GL_VIEWPORT,viewport);
    if(viewport[2]<=0||viewport[3]<=0)return {};
    if(!gluProject(0,0,0,model,projection,viewport,&px,&py,&depth))return {};
    double x[3],y[3],z[3];
    const double vx[]={double(viewport[0]),double(viewport[0]+viewport[2]),double(viewport[0])};
    const double vy[]={double(viewport[1]),double(viewport[1]),double(viewport[1]+viewport[3])};
    for(int i=0;i<3;++i)if(!gluUnProject(vx[i],vy[i],depth,model,projection,viewport,&x[i],&y[i],&z[i]))return {};
    double w=x[1]-x[0],h=y[2]-y[0],epsilon=(std::abs(w)+std::abs(h)+1)*1e-7;
    if(!std::isfinite(w)||!std::isfinite(h)||std::abs(w)<1e-8||std::abs(h)<1e-8)return {};
    if(std::abs(y[1]-y[0])>epsilon||std::abs(x[2]-x[0])>epsilon)return {};
    for(double v:z)if(!std::isfinite(v)||std::abs(v)>epsilon)return {};
    // UVs are authored with +V upward. A reversed framebuffer stores the
    // cockpit image with its row origin at the other edge. GLU only knows
    // OpenGL's window transform, not X-Plane's framebuffer orientation.
    return reverse_y?TextureRect{x[0],y[2],w,-h}:TextureRect{x[0],y[0],w,h};
}
}
