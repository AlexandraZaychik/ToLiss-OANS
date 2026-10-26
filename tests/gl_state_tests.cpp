// Real driver regression in a hidden, test-owned WGL window. Does not attach to
// X-Plane, capture the user's desktop, or require the test SDK/aircraft plugin.
#include "../src/gl_state.hpp"
#include "../src/gl_coords.hpp"
#include "../src/gl_font.hpp"
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
int checks=0;
void check(bool yes,const char* label) {++checks;if(!yes)throw std::runtime_error(label);}
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
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    try {
        Context context;context.open();
        GLuint textures[2];glGenTextures(2,textures);
        glBindTexture(GL_TEXTURE_2D,textures[0]);
        const unsigned char cyan[]={0,255,255,255};
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA,1,1,0,GL_RGBA,GL_UNSIGNED_BYTE,cyan);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);
        for(int frame=0;frame<32;++frame) {
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
        for(int scenario=0;scenario<5;++scenario) {
            glDisable(GL_CULL_FACE);glDisable(GL_COLOR_LOGIC_OP);glDisable(GL_STENCIL_TEST);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
            glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);baseline();clear();
            using Equation=void(APIENTRY*)(GLenum);
            auto equation=oans::gl_proc<Equation>("glBlendEquation");
            if(equation)equation(0x8006);
            if(scenario==0) {glEnable(GL_CULL_FACE);glCullFace(GL_FRONT);}
            if(scenario==1)glPolygonMode(GL_FRONT_AND_BACK,GL_LINE);
            if(scenario==2) {glEnable(GL_COLOR_LOGIC_OP);glLogicOp(GL_CLEAR);}
            if(scenario==3) {glEnable(GL_STENCIL_TEST);glStencilFunc(GL_NEVER,0,255);}
            if(scenario==4)glBlendFunc(GL_ZERO,GL_ZERO);
            quad();check(!red(),"inherited state reproduces missing polygon fill");
            clear();
            {oans::DrawingState state(baseline);quad();check(red(),"guard restores solid red polygon pixels on real driver");}
            GLint mode[2]={};glGetIntegerv(GL_POLYGON_MODE,mode);
            if(scenario==0)check(glIsEnabled(GL_CULL_FACE),"restore caller culling");
            if(scenario==1)check(mode[0]==GL_LINE&&mode[1]==GL_LINE,"restore caller polygon mode");
            if(scenario==2)check(glIsEnabled(GL_COLOR_LOGIC_OP),"restore caller logic operation");
            if(scenario==3)check(glIsEnabled(GL_STENCIL_TEST),"restore caller stencil");
            if(scenario==4) {GLint src,dst;glGetIntegerv(GL_BLEND_SRC,&src);glGetIntegerv(GL_BLEND_DST,&dst);check(src==GL_ZERO&&dst==GL_ZERO,"restore caller blend factors");}
        }
        glDisable(GL_CULL_FACE);glDisable(GL_COLOR_LOGIC_OP);glDisable(GL_STENCIL_TEST);glPolygonMode(GL_FRONT_AND_BACK,GL_FILL);
        glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);baseline();clear();
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
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,4096,0,4096,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        auto target=oans::texture_rectangle();check(target&&std::abs(target->w-4096)<1e-5&&std::abs(target->h-4096)<1e-5,"derive full render target coordinates independently of legacy 1024 bounds");
        clear();{
            oans::DrawingState state(baseline);glColor3f(1,0,0);glBegin(GL_QUADS);
            for(auto p:{std::pair<double,double>{.25,.25},{.75,.25},{.75,.75},{.25,.75}})glVertex2d(target->l+p.first*target->w,target->b+p.second*target->h);
            glEnd();check(red(),"full-texture UV rectangle reaches correct real framebuffer pixels");
        }
        auto reversed=oans::texture_rectangle(true);check(reversed&&std::abs(reversed->b-4096)<1e-5&&std::abs(reversed->h+4096)<1e-5,"reverse framebuffer row origin for authored cockpit UVs");
        clear();{
            oans::DrawingState state(baseline);glColor3f(1,0,0);glBegin(GL_QUADS);
            for(auto p:{std::pair<double,double>{.1,.7},{.3,.7},{.3,.9},{.1,.9}})glVertex2d(reversed->l+p.first*reversed->w,reversed->b+p.second*reversed->h);
            glEnd();unsigned char rgb[4]={};glReadPixels(6,6,1,1,GL_RGBA,GL_UNSIGNED_BYTE,rgb);check(rgb[0]>240&&rgb[1]<10,"reversed UV draws to correct asymmetric framebuffer rows");
        }
        glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,64,0,64,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        oans::AtlasFont font;check(font.load("assets/font_b612_px2.bin"),"read original licensed B612 atlas and glyph metrics");
        auto allocate=[](int* out,int n){glGenTextures(n,reinterpret_cast<GLuint*>(out));};
        auto bind=[](int id,int){glBindTexture(GL_TEXTURE_2D,static_cast<GLuint>(id));};
        check(font.upload(allocate,bind),"upload plugin-owned B612 texture");
        auto ink=[]() {unsigned char pixels[32*32*4]={};glReadPixels(0,0,32,32,GL_RGBA,GL_UNSIGNED_BYTE,pixels);int n=0;
            for(int i=0;i<32*32;++i)if(pixels[i*4]>128)++n;
            return n;};
        clear();{oans::DrawingState state(baseline);const float white[]={1,1,1};glEnable(GL_TEXTURE_2D);bind(font.texture(),0);font.draw(4,10,"OANS",white,60);glDisable(GL_TEXTURE_2D);}
        check(ink()>30,"scalable OANS lettering produces real filled glyph pixels");
        GLdouble restored_model[16];glGetDoublev(GL_MODELVIEW_MATRIX,restored_model);
        check(std::abs(restored_model[0]-1)<1e-8&&std::abs(restored_model[12])<1e-8,"text restores caller model transform despite glyph advances");
        clear();{oans::DrawingState state(baseline);const float white[]={1,1,1};glEnable(GL_TEXTURE_2D);bind(font.texture(),0);font.draw(4,10,"OANS",white,0);glDisable(GL_TEXTURE_2D);}
        check(ink()==0,"control-width clipping prevents text from bleeding beyond empty available width");
        auto clipped_text=[&](const std::string& value) {
            clear();const float white[]={1,1,1};glEnable(GL_TEXTURE_2D);bind(font.texture(),0);
            font.draw(4,10,value,white,30,12,false);glDisable(GL_TEXTURE_2D);
            std::vector<unsigned char> pixels(64*64*4);glReadPixels(0,0,64,64,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
        };
        check(clipped_text("OANS LONG NAME")==clipped_text("OAN"),"dropdown overflow matches plain prefix without ellipsis pixels");font.release();
        check(glGetError()==GL_NO_ERROR,"no OpenGL errors in guard and scalable font regression");
        std::cout<<"PASS "<<checks<<" real OpenGL pixel/state checks; driver="<<glGetString(GL_VERSION)<<'\n';return 0;
    } catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<" WindowsError="<<GetLastError()<<'\n';return 1;}
}
