#pragma once
#if defined(_WIN32)
#include <windows.h>
#include <GL/gl.h>
#include <GL/glu.h>
#elif defined(__APPLE__)
#ifndef GL_SILENCE_DEPRECATION
#define GL_SILENCE_DEPRECATION
#endif
#include <OpenGL/OpenGL.h>
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <dlfcn.h>
#ifndef APIENTRY
#define APIENTRY
#endif
#else
#error Unsupported OpenGL platform
#endif
#include <cstdint>
#include <cstring>

namespace oans {
#ifdef _WIN32
using GLContext=HGLRC;
inline GLContext current_gl_context() {return wglGetCurrentContext();}
#else
using GLContext=CGLContextObj;
inline GLContext current_gl_context() {return CGLGetCurrentContext();}
#endif
template<class T>T gl_proc(const char* name) {
#ifdef _WIN32
    auto raw=wglGetProcAddress(name);auto address=reinterpret_cast<std::uintptr_t>(raw);
    if(address<=3||address==static_cast<std::uintptr_t>(-1))return nullptr;
#else
    // X-Plane's macOS plugin bridge uses a legacy (fixed-function) context.
    // Its framebuffer and vertex-array entry points use EXT/APPLE names.
    const char* symbol=name;
    if(std::strcmp(name,"glBindVertexArray")==0)symbol="glBindVertexArrayAPPLE";
    else if(std::strcmp(name,"glGenVertexArrays")==0)symbol="glGenVertexArraysAPPLE";
    else if(std::strcmp(name,"glDeleteVertexArrays")==0)symbol="glDeleteVertexArraysAPPLE";
    else if(std::strcmp(name,"glBindFramebuffer")==0)symbol="glBindFramebufferEXT";
    else if(std::strcmp(name,"glGenFramebuffers")==0)symbol="glGenFramebuffersEXT";
    else if(std::strcmp(name,"glDeleteFramebuffers")==0)symbol="glDeleteFramebuffersEXT";
    else if(std::strcmp(name,"glFramebufferTexture2D")==0)symbol="glFramebufferTexture2DEXT";
    else if(std::strcmp(name,"glCheckFramebufferStatus")==0)symbol="glCheckFramebufferStatusEXT";
    auto raw=dlsym(RTLD_DEFAULT,symbol);
#endif
    T result=nullptr;static_assert(sizeof result==sizeof raw,"GL function pointer size");
    std::memcpy(&result,&raw,sizeof result);return result;
}
}
