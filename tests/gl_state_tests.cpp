// Real driver regression in a test-owned offscreen CGL context or hidden WGL window. Does not attach to
// X-Plane, capture the user's desktop, or require the test SDK/aircraft plugin.
#include "../src/gl_state.hpp"
#include "../src/gl_coords.hpp"
#include "../src/gl_font.hpp"
#include "../src/a350_gl.hpp"
#include <iostream>
#include "test_support.hpp"
#include <stdexcept>
#include <string>

namespace {
int checks=0;
void check(bool yes,const char* label) {++checks;if(!yes)throw std::runtime_error(label);}
#ifdef __APPLE__
struct Context {
    CGLContextObj gl=nullptr;CGLPixelFormatObj format=nullptr;GLuint fbo=0,color=0,stencil=0;
    void open() {
        CGLPixelFormatAttribute attributes[]={kCGLPFAAccelerated,kCGLPFAColorSize,static_cast<CGLPixelFormatAttribute>(24),static_cast<CGLPixelFormatAttribute>(0)};
        GLint count=0;check(CGLChoosePixelFormat(attributes,&format,&count)==kCGLNoError&&count,"choose CGL pixel format");
        check(CGLCreateContext(format,nullptr,&gl)==kCGLNoError&&CGLSetCurrentContext(gl)==kCGLNoError,"create real driver context");
        glGenFramebuffersEXT(1,&fbo);glBindFramebufferEXT(GL_FRAMEBUFFER_EXT,fbo);
        glGenRenderbuffersEXT(1,&color);glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,color);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_RGBA8,64,64);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_COLOR_ATTACHMENT0_EXT,GL_RENDERBUFFER_EXT,color);
        glGenRenderbuffersEXT(1,&stencil);glBindRenderbufferEXT(GL_RENDERBUFFER_EXT,stencil);
        glRenderbufferStorageEXT(GL_RENDERBUFFER_EXT,GL_STENCIL_INDEX8_EXT,64,64);
        glFramebufferRenderbufferEXT(GL_FRAMEBUFFER_EXT,GL_STENCIL_ATTACHMENT_EXT,GL_RENDERBUFFER_EXT,stencil);
        check(glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT)==GL_FRAMEBUFFER_COMPLETE_EXT,"complete offscreen framebuffer");
        glViewport(0,0,32,32);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,0,1,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    }
    ~Context() {
        if(gl) {CGLSetCurrentContext(gl);glDeleteRenderbuffersEXT(1,&stencil);glDeleteRenderbuffersEXT(1,&color);glDeleteFramebuffersEXT(1,&fbo);CGLSetCurrentContext(nullptr);CGLDestroyContext(gl);}
        if(format)CGLDestroyPixelFormat(format);
    }
};
#else
struct Context {
    HWND window=nullptr;HDC dc=nullptr;HGLRC gl=nullptr;
    void open() {
        WNDCLASSW c{};c.style=CS_OWNDC;c.lpfnWndProc=DefWindowProcW;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"ToLissOANS_GLRegression";
        check(RegisterClassW(&c)!=0,"register hidden test window");
        window=CreateWindowW(c.lpszClassName,L"OpenGL regression",WS_OVERLAPPEDWINDOW,0,0,64,64,nullptr,nullptr,c.hInstance,nullptr);
        check(window!=nullptr,"create hidden test window");dc=GetDC(window);
        PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof p;p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;
        p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=24;p.cAlphaBits=8;p.cStencilBits=8;
        int format=ChoosePixelFormat(dc,&p);check(format!=0&&SetPixelFormat(dc,format,&p),"set test pixel format");
        gl=wglCreateContext(dc);check(gl&&wglMakeCurrent(dc,gl),"create real driver context");
        glViewport(0,0,32,32);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,0,1,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
    }
    ~Context() {if(gl) {wglMakeCurrent(nullptr,nullptr);wglDeleteContext(gl);}if(dc)ReleaseDC(window,dc);if(window)DestroyWindow(window);UnregisterClassW(L"ToLissOANS_GLRegression",GetModuleHandleW(nullptr));}
};
#endif
void baseline() {
    glDisable(GL_FOG);glDisable(GL_TEXTURE_2D);glDisable(GL_LIGHTING);glDisable(GL_ALPHA_TEST);
    glEnable(GL_BLEND);glDisable(GL_DEPTH_TEST);glDepthMask(GL_FALSE);
}
int restored_sdk[7]={},restored_texture=0;
void restore_sdk(const int* s,int texture) {
    for(int i=0;i<7;++i)restored_sdk[i]=s[i];
    restored_texture=texture;
    const GLenum flags[]={GL_FOG,GL_TEXTURE_2D,GL_LIGHTING,GL_ALPHA_TEST,GL_BLEND,GL_DEPTH_TEST};
    for(int i=0;i<6;++i) {if(s[i])glEnable(flags[i]);else glDisable(flags[i]);}
    glDepthMask(s[6]?GL_TRUE:GL_FALSE);glBindTexture(GL_TEXTURE_2D,texture);
}
void quad() {
    glColor3f(1,0,0);glBegin(GL_QUADS);glVertex2f(.1f,.1f);glVertex2f(.9f,.1f);glVertex2f(.9f,.9f);glVertex2f(.1f,.9f);glEnd();
}
bool red() {unsigned char rgb[3]={};glFinish();glReadPixels(16,16,1,1,GL_RGB,GL_UNSIGNED_BYTE,rgb);return rgb[0]>240&&rgb[1]<10&&rgb[2]<10;}
void clear() {glDisable(GL_SCISSOR_TEST);glColorMask(1,1,1,1);glClearColor(0,0,0,0);glClear(GL_COLOR_BUFFER_BIT|GL_STENCIL_BUFFER_BIT);}
}
int main() {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
#endif
    try {
        Context context;context.open();
        GLuint textures[2];glGenTextures(2,textures);
        glBindTexture(GL_TEXTURE_2D,textures[0]);
        const unsigned char cyan[]={0,255,255,255};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,cyan);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        scenario("gl/consecutive-textured-panels");
        for(int frame=0;frame<2;++frame) {
            baseline();clear();glEnable(GL_TEXTURE_2D);glEnable(GL_ALPHA_TEST);
            glDisable(GL_BLEND);glDepthMask(GL_TRUE);glBindTexture(GL_TEXTURE_2D,textures[0]);
            {oans::DrawingState state(baseline,restore_sdk);quad();check(red(),"overlay draws independently of inherited textured panel state");glBindTexture(GL_TEXTURE_2D,textures[1]);}
            check(restored_sdk[1]==1&&restored_sdk[3]==1&&restored_sdk[4]==0&&restored_sdk[6]==1&&restored_texture==static_cast<int>(textures[0]),"SDK restore receives original state and panel texture");
            GLboolean depth=GL_FALSE;glGetBooleanv(GL_DEPTH_WRITEMASK,&depth);
            check(glIsEnabled(GL_TEXTURE_2D)&&glIsEnabled(GL_ALPHA_TEST)&&!glIsEnabled(GL_BLEND)&&depth,"preserve panel texture alpha blend and depth write state");
            glColor3f(1,1,1);glBegin(GL_QUADS);glTexCoord2f(.5f,.5f);
            glVertex2f(.1f,.1f);glVertex2f(.9f,.1f);glVertex2f(.9f,.9f);glVertex2f(.1f,.9f);glEnd();
            unsigned char rgb[3]={};glReadPixels(16,16,1,1,GL_RGB,GL_UNSIGNED_BYTE,rgb);
            check(rgb[0]<10&&rgb[1]>240&&rgb[2]>240,"next aircraft panel draw keeps original texture pixels over consecutive frames");
        }
        glDeleteTextures(2,textures);baseline();
        glDisable(GL_CULL_FACE);glDisable(GL_COLOR_LOGIC_OP);glDisable(GL_STENCIL_TEST);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);baseline();clear();
        scenario("gl/inherited-shader");
        using CreateShader=GLuint(APIENTRY*)(GLenum);
        using Source=void(APIENTRY*)(GLuint,GLsizei,const char*const*,const GLint*);
        using Object=void(APIENTRY*)(GLuint);
        using GetObject=void(APIENTRY*)(GLuint,GLenum,GLint*);
        using CreateProgram=GLuint(APIENTRY*)();
        using Attach=void(APIENTRY*)(GLuint,GLuint);
        auto create_shader=oans::gl_proc<CreateShader>("glCreateShader");auto source=oans::gl_proc<Source>("glShaderSource");
        auto compile=oans::gl_proc<Object>("glCompileShader");auto shader_status=oans::gl_proc<GetObject>("glGetShaderiv");
        auto create_program=oans::gl_proc<CreateProgram>("glCreateProgram");auto attach=oans::gl_proc<Attach>("glAttachShader");
        auto link=oans::gl_proc<Object>("glLinkProgram");auto program_status=oans::gl_proc<GetObject>("glGetProgramiv");
        auto use_program=oans::gl_proc<Object>("glUseProgram");
        check(create_shader&&source&&compile&&shader_status&&create_program&&attach&&link&&program_status&&use_program,"driver supports shader contamination regression");
        const char* shaders[]={"#version 120\nvoid main(){gl_Position=gl_ModelViewProjectionMatrix*gl_Vertex;}","#version 120\nvoid main(){gl_FragColor=vec4(0.0,0.0,0.0,1.0);}"};
        GLuint objects[2];
        for(int i=0;i<2;++i) {objects[i]=create_shader(i==0?0x8B31:0x8B30);source(objects[i],1,&shaders[i],nullptr);compile(objects[i]);GLint ok=0;shader_status(objects[i],0x8B81,&ok);check(ok==1,"compile test shader");}
        auto program=create_program();for(auto object:objects)attach(program,object);link(program);GLint linked=0;program_status(program,0x8B82,&linked);check(linked==1,"link test program");
        use_program(program);quad();check(!red(),"inherited shader reproduces missing red geometry");
        clear();{oans::DrawingState state(baseline);quad();check(red(),"guard renders correct pixels despite inherited shader");}
        GLint restored=0;glGetIntegerv(0x8B8D,&restored);check(static_cast<GLuint>(restored)==program,"restore caller shader program");use_program(0);
        oans::gl_proc<Object>("glDeleteProgram")(program);for(auto object:objects)oans::gl_proc<Object>("glDeleteShader")(object);
        scenario("gl/large-cockpit-uv");
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,4096,0,4096,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        auto target=oans::texture_rectangle();check(target&&std::abs(target->w-4096)<1e-5&&std::abs(target->h-4096)<1e-5,"derive full render target coordinates independently of legacy 1024 bounds");
        GLdouble captured_model[16],captured_projection[16];GLint captured_viewport[4];
        glGetDoublev(GL_MODELVIEW_MATRIX,captured_model);glGetDoublev(GL_PROJECTION_MATRIX,captured_projection);glGetIntegerv(GL_VIEWPORT,captured_viewport);
        auto captured=oans::texture_rectangle(captured_model,captured_projection,captured_viewport);
        check(captured&&captured->l==target->l&&captured->b==target->b&&captured->w==target->w&&captured->h==target->h,"captured transform matches live-query rectangle");
        const auto captured_reverse=oans::texture_rectangle(captured_model,captured_projection,captured_viewport,true);
        check(captured_reverse&&captured_reverse->b==target->b+target->h&&captured_reverse->h==-target->h,"captured transform preserves reversed framebuffer mapping");
        captured_viewport[2]=0;
        check(!oans::texture_rectangle(captured_model,captured_projection,captured_viewport),"captured transform rejects invalid viewport");
        clear();{
            oans::DrawingState state(baseline);glColor3f(1,0,0);glBegin(GL_QUADS);
            for(auto p:{std::pair<double,double>{.25,.25},{.75,.25},{.75,.75},{.25,.75}})glVertex2d(target->l+p.first*target->w,target->b+p.second*target->h);
            glEnd();check(red(),"full-texture UV rectangle reaches correct real framebuffer pixels");
        }
        scenario("gl/reversed-cockpit-uv");
        auto reversed=oans::texture_rectangle(true);check(reversed&&std::abs(reversed->b-4096)<1e-5&&std::abs(reversed->h+4096)<1e-5,"reverse framebuffer row origin for authored cockpit UVs");
        clear();{
            oans::DrawingState state(baseline);glColor3f(1,0,0);glBegin(GL_QUADS);
            for(auto p:{std::pair<double,double>{.1,.7},{.3,.7},{.3,.9},{.1,.9}})glVertex2d(reversed->l+p.first*reversed->w,reversed->b+p.second*reversed->h);
            glEnd();unsigned char rgb[4]={};glReadPixels(6,6,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgb);check(rgb[0]>240&&rgb[1]<10,"reversed UV draws to correct asymmetric framebuffer rows");
        }

        scenario("gl/batch-dimming-and-client-state");
        glViewport(0,0,32,32);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1,0,1,-1,1);
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();baseline();glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        std::vector<oans::A350Vertex> many;
        for(int i=0;i<3000;++i)for(auto p:std::array<std::pair<float,float>,3>{{{.1f,.1f},{.9f,.1f},{.5f,.9f}}})
            many.push_back({p.first,p.second,p.first,p.second,0xB1AB7953u});
        auto read=[](){std::array<unsigned char,32*32*4> p{};glFinish();glReadPixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,p.data());return p;};
        for(double gain:{0.,.01,.37,.999,1.}) {
            clear();oans::a350_gl_detail::immediate(many,gain);const auto expected=read();
            GLfloat expected_color[4];glGetFloatv(GL_CURRENT_COLOR,expected_color);
            const GLfloat expected_uv[]={many.back().u,many.back().v,0,1};
            clear();check(oans::a350_gl_detail::batch(many,gain),"real driver uses batch path");
            check(read()==expected,"batch keeps dimmed RGB and alpha pixels byte exact");
            GLfloat color[4],uv[4];glGetFloatv(GL_CURRENT_COLOR,color);glGetFloatv(GL_CURRENT_TEXTURE_COORDS,uv);
            for(int i=0;i<4;++i)check(color[i]==expected_color[i]&&uv[i]==expected_uv[i],"current attributes explicitly match final vertex");
        }
        using BindBuffer=void(APIENTRY*)(GLenum,GLuint);
        using GenObjects=void(APIENTRY*)(GLsizei,GLuint*);
        using DeleteObjects=void(APIENTRY*)(GLsizei,const GLuint*);
        using BufferData=void(APIENTRY*)(GLenum,std::ptrdiff_t,const void*,GLenum);
        using ClientTexture=void(APIENTRY*)(GLenum);
        auto bind_buffer=oans::gl_proc<BindBuffer>("glBindBuffer");auto gen_buffers=oans::gl_proc<GenObjects>("glGenBuffers");
        auto delete_buffers=oans::gl_proc<DeleteObjects>("glDeleteBuffers");auto buffer_data=oans::gl_proc<BufferData>("glBufferData");
        auto client_texture=oans::gl_proc<ClientTexture>("glClientActiveTexture");
        auto bind_vao=oans::gl_proc<void(APIENTRY*)(GLuint)>("glBindVertexArray");auto gen_vaos=oans::gl_proc<GenObjects>("glGenVertexArrays");
        auto delete_vaos=oans::gl_proc<DeleteObjects>("glDeleteVertexArrays");
        check(bind_buffer&&gen_buffers&&delete_buffers&&buffer_data&&client_texture,"buffer and multitexture regression supported");
        clear();oans::a350_gl_detail::immediate(many,.37);const auto expected=read();
        GLuint buffer=0,vao=0;gen_buffers(1,&buffer);
        if(bind_vao&&gen_vaos&&delete_vaos){gen_vaos(1,&vao);bind_vao(vao);}
        bind_buffer(0x8892,buffer);const float dummy[16]={};buffer_data(0x8892,sizeof dummy,dummy,0x88E4);
        glVertexPointer(3,GL_FLOAT,16,reinterpret_cast<const void*>(4));glEnableClientState(GL_VERTEX_ARRAY);
        glColorPointer(4,GL_UNSIGNED_BYTE,16,reinterpret_cast<const void*>(8));glEnableClientState(GL_COLOR_ARRAY);
        glNormalPointer(GL_FLOAT,16,nullptr);glEnableClientState(GL_NORMAL_ARRAY);
        glIndexPointer(GL_FLOAT,16,nullptr);glEnableClientState(GL_INDEX_ARRAY);
        glEdgeFlagPointer(16,nullptr);glEnableClientState(GL_EDGE_FLAG_ARRAY);
        for(int unit=0;unit<2;++unit){client_texture(0x84C0+unit);glTexCoordPointer(3,GL_FLOAT,16,reinterpret_cast<const void*>(4));glEnableClientState(GL_TEXTURE_COORD_ARRAY);}
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_REPLACE);
        clear();oans::draw_a350_vertices(many,.37);check(read()==expected,"auto batch ignores contaminated client arrays/VBO/VAO");
        GLint value=0;glGetIntegerv(0x8894,&value);check(value==static_cast<GLint>(buffer),"restore array buffer binding");
        glGetIntegerv(0x84E1,&value);check(value==0x84C1,"restore client active texture");
        glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&value);check(value==GL_REPLACE,"restore texture environment");
        if(vao){glGetIntegerv(0x85B5,&value);check(value==static_cast<GLint>(vao),"restore inherited VAO");}
        void* pointer=nullptr;glGetPointerv(GL_VERTEX_ARRAY_POINTER,&pointer);check(pointer==reinterpret_cast<void*>(4),"restore vertex pointer offset");
        glGetIntegerv(GL_VERTEX_ARRAY_SIZE,&value);check(value==3,"restore vertex format");
        for(auto state:{GL_VERTEX_ARRAY,GL_COLOR_ARRAY,GL_NORMAL_ARRAY,GL_INDEX_ARRAY,GL_EDGE_FLAG_ARRAY})check(glIsEnabled(state),"restore conventional array enables");
        for(int unit=0;unit<2;++unit){client_texture(0x84C0+unit);glGetPointerv(GL_TEXTURE_COORD_ARRAY_POINTER,&pointer);check(glIsEnabled(GL_TEXTURE_COORD_ARRAY)&&pointer==reinterpret_cast<void*>(4),"restore each texture unit's coordinate array");}
        client_texture(0x84C0);if(vao){bind_vao(0);delete_vaos(1,&vao);}
        bind_buffer(0x8892,0);delete_buffers(1,&buffer);
        for(auto state:{GL_VERTEX_ARRAY,GL_COLOR_ARRAY,GL_NORMAL_ARRAY,GL_INDEX_ARRAY,GL_EDGE_FLAG_ARRAY,GL_TEXTURE_COORD_ARRAY})glDisableClientState(state);
        client_texture(0x84C1);glDisableClientState(GL_TEXTURE_COORD_ARRAY);client_texture(0x84C0);
        glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        GLint depth=0,limit=0;glGetIntegerv(GL_CLIENT_ATTRIB_STACK_DEPTH,&depth);glGetIntegerv(GL_MAX_CLIENT_ATTRIB_STACK_DEPTH,&limit);
        clear();oans::a350_gl_detail::immediate(many,1);const auto fallback_pixels=read();
        for(int i=depth;i<limit;++i)glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
        clear();check(!oans::a350_gl_detail::batch(many,1),"full client stack rejects batch before mutation");
        oans::draw_a350_vertices(many,1);check(read()==fallback_pixels,"stack-full fallback preserves original pixels");
        for(int i=depth;i<limit;++i)glPopClientAttrib();
        const std::vector<oans::A350Vertex> small(many.begin(),many.begin()+3);
        clear();oans::a350_gl_detail::immediate(small,.37);const auto small_pixels=read();
        clear();oans::draw_a350_vertices(small,.37);check(read()==small_pixels,"small-buffer immediate branch preserves pixels");
        const auto prior=read();oans::draw_a350_vertices({},1);check(read()==prior,"empty buffer has no drawing side effects");

        check(glGetError()==GL_NO_ERROR,"no OpenGL errors in guard and scalable font regression");
        std::cout<<"PASS "<<checks<<" real OpenGL pixel/state checks; driver="<<glGetString(GL_VERSION)<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
