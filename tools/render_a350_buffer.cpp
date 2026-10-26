// Hidden test-owned WGL surface: render the SAME buffer submission as .xpl.
// This is an offline rendering preview, never a screenshot of X-Plane.
#include "a350_gl.hpp"
#include "gl_font.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
namespace {
struct Context {
    HWND window=nullptr;HDC dc=nullptr;HGLRC gl=nullptr;
    Context() {
        WNDCLASSW c{};c.style=CS_OWNDC;c.lpfnWndProc=DefWindowProcW;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"A350MapOfflinePreview";
        if(!RegisterClassW(&c))throw std::runtime_error("Cannot register preview surface");
        window=CreateWindowW(c.lpszClassName,L"Offline A350 map preview",WS_POPUP,0,0,600,600,nullptr,nullptr,c.hInstance,nullptr);
        dc=GetDC(window);PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof p;p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;
        p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=24;p.cAlphaBits=8;
        int format=ChoosePixelFormat(dc,&p);
        if(!format||!SetPixelFormat(dc,format,&p))throw std::runtime_error("Cannot set preview pixel format");
        gl=wglCreateContext(dc);if(!gl||!wglMakeCurrent(dc,gl))throw std::runtime_error("Cannot create preview GL context");
    }
    ~Context(){if(gl){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(gl);}if(dc)ReleaseDC(window,dc);if(window)DestroyWindow(window);}
};
void allocate(int* n,int count){glGenTextures(count,reinterpret_cast<GLuint*>(n));}
void bind(int n,int){glBindTexture(GL_TEXTURE_2D,n);}
}
int main(int argc,char** argv) {
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    if(argc!=4){std::cerr<<"Usage: render_a350_buffer BUFFER ATLAS OUTPUT.bmp\n";return 2;}
    try {
        std::ifstream input(std::filesystem::u8path(argv[1]),std::ios::binary|std::ios::ate);
        auto bytes=input.tellg();if(bytes<0||bytes%sizeof(oans::A350Vertex))throw std::runtime_error("Invalid buffer");
        std::vector<oans::A350Vertex> vertices(static_cast<size_t>(bytes)/sizeof(oans::A350Vertex));
        input.seekg(0);input.read(reinterpret_cast<char*>(vertices.data()),bytes);
        Context ctx;oans::AtlasFont font;if(!font.load(std::filesystem::u8path(argv[2]))||!font.upload(allocate,bind))throw std::runtime_error("Atlas upload failed");
        glViewport(0,0,600,600);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,600,0,600,-1,1);
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
        glEnable(GL_TEXTURE_2D);bind(font.texture(),0);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);oans::draw_a350_vertices(vertices);glFinish();
        std::vector<unsigned char> rgb(600*600*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,600,600,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
        if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GL render/readback failed");
        size_t lit=0;for(size_t i=0;i<rgb.size();i+=3){if(rgb[i]||rgb[i+1]||rgb[i+2])++lit;std::swap(rgb[i],rgb[i+2]);}
        if(lit<100)throw std::runtime_error("Empty render");
        BITMAPFILEHEADER head{};head.bfType=0x4D42;head.bfOffBits=sizeof head+sizeof(BITMAPINFOHEADER);head.bfSize=head.bfOffBits+static_cast<DWORD>(rgb.size());
        BITMAPINFOHEADER info{};info.biSize=sizeof info;info.biWidth=600;info.biHeight=600;info.biPlanes=1;info.biBitCount=24;info.biSizeImage=static_cast<DWORD>(rgb.size());
        std::ofstream out(std::filesystem::u8path(argv[3]),std::ios::binary);out.write(reinterpret_cast<const char*>(&head),sizeof head);out.write(reinterpret_cast<const char*>(&info),sizeof info);out.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());
        if(!out)throw std::runtime_error("Cannot write preview");
        std::cout<<"Rendered "<<vertices.size()<<" original A350 vertices; nonblack_pixels="<<lit<<" GL="<<glGetString(GL_VERSION)<<'\n';font.release();
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
