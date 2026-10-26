#pragma once
#include <windows.h>
#include <GL/gl.h>
#include <cstdint>
#include <cstring>

namespace oans {
template<class T>T gl_proc(const char* name) {
    auto raw=wglGetProcAddress(name);auto address=reinterpret_cast<std::uintptr_t>(raw);
    if(address<=3||address==static_cast<std::uintptr_t>(-1))return nullptr;
    T result=nullptr;static_assert(sizeof result==sizeof raw,"Windows GL function pointer size");
    std::memcpy(&result,&raw,sizeof result);return result;
}
// XPLM controls only part of the inherited OpenGL state. In particular, panel
// drawing may enter with culling, logic operations, stencil or wireframe active.
// SDK callers supply both baseline and restoration hooks to keep its cache
// synchronized with the inherited GL state. Raw WGL tests need no SDK hook.
class DrawingState {
    using BlendEquation=void (APIENTRY*)(GLenum);
    using BindObject=void (APIENTRY*)(GLuint);
    using BindBuffer=void (APIENTRY*)(GLenum,GLuint);
    void (*baseline_)();
    void (*restore_)(const int*,int)=nullptr;
    int sdk_[7]={},texture_=0;
    BindObject use_program_=nullptr,bind_vao_=nullptr;
    BindBuffer bind_buffer_=nullptr;
    GLint program_=0,vao_=0,array_buffer_=0;
public:
    explicit DrawingState(void (*baseline)(),void (*restore)(const int*,int)=nullptr):baseline_(baseline),restore_(restore) {
        // Save BEFORE the SDK baseline: aircraft panel code can inherit this
        // callback's state on the next draw (including backup speed/altitude).
        glPushAttrib(GL_ALL_ATTRIB_BITS);
        sdk_[0]=glIsEnabled(GL_FOG);sdk_[2]=glIsEnabled(GL_LIGHTING);
        sdk_[3]=glIsEnabled(GL_ALPHA_TEST);sdk_[4]=glIsEnabled(GL_BLEND);
        sdk_[5]=glIsEnabled(GL_DEPTH_TEST);
        GLboolean depth_write=GL_FALSE;glGetBooleanv(GL_DEPTH_WRITEMASK,&depth_write);sdk_[6]=depth_write;
        using ActiveTexture=void(APIENTRY*)(GLenum);
        auto active=gl_proc<ActiveTexture>("glActiveTexture");
        GLint unit=0x84C0,units=1;
        if(active) {glGetIntegerv(0x84E0,&unit);glGetIntegerv(0x84E2,&units);}
        for(int i=0;i<units;++i) {
            if(active)active(0x84C0+i);
            if(glIsEnabled(GL_TEXTURE_2D))sdk_[1]=i+1;
            if(i==0)glGetIntegerv(GL_TEXTURE_BINDING_2D,&texture_);
        }
        if(active)active(unit);
        baseline_();
        use_program_=gl_proc<BindObject>("glUseProgram");bind_vao_=gl_proc<BindObject>("glBindVertexArray");
        bind_buffer_=gl_proc<BindBuffer>("glBindBuffer");
        if(use_program_) {glGetIntegerv(0x8B8D,&program_);if(program_)use_program_(0);}
        if(bind_vao_) {glGetIntegerv(0x85B5,&vao_);if(vao_)bind_vao_(0);}
        if(bind_buffer_) {glGetIntegerv(0x8894,&array_buffer_);if(array_buffer_)bind_buffer_(0x8892,0);}
        glDisable(GL_CULL_FACE);glDisable(GL_COLOR_LOGIC_OP);glDisable(GL_STENCIL_TEST);
        glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        auto blend_equation=gl_proc<BlendEquation>("glBlendEquation");
        if(blend_equation)blend_equation(0x8006); // GL_FUNC_ADD, OpenGL 1.4
    }
    ~DrawingState() {
        baseline_();
        // Reconcile XPLM's state/texture caches before restoring the remaining
        // raw GL state. A glPopAttrib alone would leave those caches stale.
        if(restore_)restore_(sdk_,texture_);
        glPopAttrib();
        if(bind_vao_)bind_vao_(static_cast<GLuint>(vao_));
        if(bind_buffer_)bind_buffer_(0x8892,static_cast<GLuint>(array_buffer_));
        if(use_program_)use_program_(static_cast<GLuint>(program_));
    }
    DrawingState(const DrawingState&)=delete;
    DrawingState& operator=(const DrawingState&)=delete;
};
}
