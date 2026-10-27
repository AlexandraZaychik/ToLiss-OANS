#pragma once
#include "a350_gl.hpp"
#include "map_reprojection.hpp"

namespace oans {
// An opaque black-backed map: RGB is already composited with its antialiasing.
// Draw at gain=1 into the FBO, then apply the current display brightness once.
class MapTexture {
    using Bind=void(APIENTRY*)(GLenum,GLuint);
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);
    using Attach=void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
    using Status=GLenum(APIENTRY*)(GLenum);
    GLuint texture_=0,fbo_=0;
    GLContext context_=nullptr;
    std::uint64_t generation_=0;
    bool valid_=false;
public:
    void invalidate(){valid_=false;}
    void release() {
        if(context_&&current_gl_context()==context_) {
            if(texture_)glDeleteTextures(1,&texture_);
            auto del=gl_proc<void(APIENTRY*)(GLsizei,const GLuint*)>("glDeleteFramebuffers");
            if(fbo_&&del)del(1,&fbo_);
        }
        texture_=fbo_=0;context_=nullptr;valid_=false;
    }
    bool update(const std::vector<A350Vertex>& vertices,GLuint atlas,std::uint64_t generation,
                void(*allocate)(int*,int)) {
        if(!current_gl_context())return false;
        if(context_!=current_gl_context())release();
        if(valid_&&generation_==generation)return true;
        auto bind=gl_proc<Bind>("glBindFramebuffer");
        auto gen=gl_proc<Gen>("glGenFramebuffers");
        auto attach=gl_proc<Attach>("glFramebufferTexture2D");
        auto status=gl_proc<Status>("glCheckFramebufferStatus");
        if(!bind||!gen||!attach||!status)return false;
        GLint draw=0,read=0,mode=0;
        glGetIntegerv(0x8CA6,&draw);glGetIntegerv(0x8CAA,&read);glGetIntegerv(GL_MATRIX_MODE,&mode);
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        if(!texture_) {
            int id=0;allocate(&id,1);texture_=static_cast<GLuint>(id);
            glBindTexture(GL_TEXTURE_2D,texture_);
            GLint unpack=0;glGetIntegerv(0x88EF,&unpack);
            auto buffer=gl_proc<Bind>("glBindBuffer");
            if(unpack&&buffer)buffer(0x88EC,0);
            glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,1024,1024,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
            if(unpack&&buffer)buffer(0x88EC,unpack);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
            gen(1,&fbo_);context_=current_gl_context();
        }
        if(!texture_||!fbo_){glPopAttrib();return false;}
        bind(0x8D40,fbo_);attach(0x8D40,0x8CE0,GL_TEXTURE_2D,texture_,0);
        glDrawBuffer(0x8CE0);glReadBuffer(0x8CE0);
        const bool complete=status(0x8D40)==0x8CD5;
        if(complete) {
            // X-Plane can enter panel callbacks with upper-left clip origin.
            using Clip=void(APIENTRY*)(GLenum,GLenum);
            auto clip=gl_proc<Clip>("glClipControl");GLint origin=0,depth=0;
            if(clip){glGetIntegerv(0x935C,&origin);glGetIntegerv(0x935D,&depth);clip(0x8CA1,depth);}
            glViewport(0,0,1024,1024);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);
            glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_ALPHA_TEST);
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
            glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
            // Keep alpha opaque: RGB is already antialiased over black. Writing
            // srcAlpha^2 would blend edge pixels a second time when compositing.
            glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_FALSE);
            glMatrixMode(GL_PROJECTION);glPushMatrix();glLoadIdentity();
            glOrtho(-map_texture_margin,600+map_texture_margin,-map_texture_margin,600+map_texture_margin,-1,1);
            glMatrixMode(GL_MODELVIEW);glPushMatrix();glLoadIdentity();
            glEnable(GL_TEXTURE_2D);glBindTexture(GL_TEXTURE_2D,atlas);
            glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
            draw_a350_vertices(vertices);
            glPopMatrix();glMatrixMode(GL_PROJECTION);glPopMatrix();glMatrixMode(mode);
            if(clip)clip(origin,depth);
        }
        bind(0x8CA9,draw);bind(0x8CA8,read);glPopAttrib();
        valid_=complete;generation_=generation;return valid_;
    }
    void draw(double gain,void(*bind)(int,int),const A350View& source,const A350View& current) const {
        bind(static_cast<int>(texture_),0);
        GLint env;glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&env);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        glColor4d(gain,gain,gain,1);glBegin(GL_QUADS);
        for(auto p:std::array<Vec2,4>{{{0,0},{600,0},{600,600},{0,600}}}) {
            const auto uv=map_cached_point(p,source,current);
            glTexCoord2d((uv.x+map_texture_margin)/(600+2*map_texture_margin),
                         (uv.y+map_texture_margin)/(600+2*map_texture_margin));
            glVertex2d(p.x,p.y);
        }
        glEnd();glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,env);
    }
    static void draw_fallback(const std::vector<A350Vertex>& vertices,double gain,
                              const A350View& source,const A350View& current) {
        // The same reprojection if the driver's FBO path is unavailable.
        const auto origin=map_cached_point({0,0},current,source);
        const auto x=map_cached_point({1,0},current,source)-origin;
        const auto y=map_cached_point({0,1},current,source)-origin;
        const GLdouble matrix[]={x.x,x.y,0,0,y.x,y.y,0,0,0,0,1,0,origin.x,origin.y,0,1};
        GLint mode;glGetIntegerv(GL_MATRIX_MODE,&mode);glMatrixMode(GL_MODELVIEW);
        glPushMatrix();glMultMatrixd(matrix);draw_a350_vertices(vertices,gain);glPopMatrix();glMatrixMode(mode);
    }
};
}
