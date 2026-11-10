#include "map_texture.hpp"
#include "gl_font.hpp"
#include "map_viewport.hpp"
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <stdexcept>

namespace {
void require(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
struct Context {
    HWND window=nullptr;HDC dc=nullptr;HGLRC gl=nullptr;
    Context() {
        WNDCLASSW c{};c.style=CS_OWNDC;c.lpfnWndProc=DefWindowProcW;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"OANS_TextureProbe";
        RegisterClassW(&c);window=CreateWindowW(c.lpszClassName,L"Offline map test",WS_POPUP,0,0,600,600,nullptr,nullptr,c.hInstance,nullptr);
        dc=GetDC(window);PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof p;p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;
        p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=24;p.cAlphaBits=8;
        int format=ChoosePixelFormat(dc,&p);require(format&&SetPixelFormat(dc,format,&p),"pixel format");
        gl=wglCreateContext(dc);require(gl&&wglMakeCurrent(dc,gl),"WGL context");
    }
    ~Context(){if(gl){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(gl);}if(dc)ReleaseDC(window,dc);if(window)DestroyWindow(window);}
};
void allocate(int* n,int count){glGenTextures(count,reinterpret_cast<GLuint*>(n));}
void bind(int id,int){glBindTexture(GL_TEXTURE_2D,id);}
void baseline() {
    glViewport(0,0,600,600);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,600,0,600,-1,1);
    glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);
    glDisable(GL_SCISSOR_TEST);glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);
    glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
}
std::vector<unsigned char> pixels(){std::vector<unsigned char> rgb(600*600*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,600,600,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());return rgb;}
void bmp(const std::filesystem::path& path,std::vector<unsigned char> rgb) {
    for(size_t i=0;i<rgb.size();i+=3)std::swap(rgb[i],rgb[i+2]);
    BITMAPFILEHEADER head{};head.bfType=0x4D42;head.bfOffBits=sizeof head+sizeof(BITMAPINFOHEADER);head.bfSize=head.bfOffBits+DWORD(rgb.size());
    BITMAPINFOHEADER info{};info.biSize=sizeof info;info.biWidth=info.biHeight=600;info.biPlanes=1;info.biBitCount=24;
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<const char*>(&head),sizeof head);out.write(reinterpret_cast<const char*>(&info),sizeof info);out.write(reinterpret_cast<const char*>(rgb.data()),rgb.size());
}
}

#include <numeric>
#include <algorithm>
using Clock=std::chrono::steady_clock;
double ms(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
int main(int argc,char** argv) {
 try {
    require(argc==6,"ASSETS XPLANE ICAO BUILDINGS OUTPUT_CSV");
    SetThreadAffinityMask(GetCurrentThread(),1);
    std::vector<oans::IndexEntry> index;
    for(auto& f:oans::scenery_files(std::filesystem::u8path(argv[2]))){auto part=oans::scan_index(f);index.insert(index.end(),part.begin(),part.end());}
    auto e=oans::find_airport(oans::canonical_index(index),argv[3]);require(bool(e),"airport missing");
    auto apt=oans::load_airport(*e);
    oans::A350Map map(std::filesystem::u8path(argv[1]),*e,std::filesystem::u8path(argv[4]));
    auto stats=map.stats();require(stats.terminals+stats.buildings>0,"DSF buildings missing");
    Context context;oans::AtlasFont font;require(font.load(std::filesystem::u8path(argv[1])/"font_b612_px2.bin")&&font.upload(allocate,bind),"font");
    std::ofstream out(std::filesystem::u8path(argv[5]));
    out<<"airport,quality,mode,range,layer,vertices,gain,immediate_cpu_ms,production_cpu_ms\n";
    auto pose=oans::local(apt.reference.lat,apt.reference.lon,apt.lat,apt.lon);double best=1e100,heading=0;oans::Vec2 chosen=pose;
    for(auto& edge:apt.edges)if(!edge.runway&&apt.nodes.count(edge.a)&&apt.nodes.count(edge.b)){
        auto a=apt.nodes.at(edge.a),b=apt.nodes.at(edge.b);double score=oans::length((a+b)*.5-pose);
        if(score<best&&oans::length(b-a)>30){best=score;chosen=(a+b)*.5;heading=std::atan2((b-a).x,(b-a).y)*180/oans::pi;}}
    pose=chosen;int pixel_checks=0,batch_checks=0;
    for(int q:{4,0})for(int mode:{2,3,4})for(double range:{.2,.5,2.}) {
        oans::A350View v;v.detail_quality=q;v.mode=mode;v.range=range;v.psi=v.mag=heading;
        auto vp=oans::map_viewport(mode,false);v.anchor=vp.anchor;v.radius=vp.radius;v.bottom=vp.bottom;v.scale=vp.radius/(range*1852);v.angle=mode==4?0:heading*oans::pi/180;
        v.lat=apt.lat+pose.y/oans::earth*180/oans::pi;v.lon=apt.lon+pose.x/(oans::earth*std::cos(apt.lat*oans::pi/180))*180/oans::pi;
        v.center=oans::local(v.lat,v.lon,map.ref_lat(),map.ref_lon());
        for(int i=0;i<8;++i)v.traffic.push_back({i+1,v.lat+(i/4-1)*.0002,v.lon+(i%4-2)*.0002,heading+i*20,"TST"+std::to_string(i),"A320",15});
        std::vector<std::pair<std::string,std::vector<oans::A350Vertex>>> buffers;
        buffers.push_back({"surface",map.render_base(0,v,0,true)});
        buffers.push_back({"labels",map.base_labels(0)});buffers.push_back({"dynamic",map.render_dynamic(v)});
        for(auto& item:buffers)for(double gain:{0.,.37,.999,1.}) {
            baseline();bind(font.texture(),0);oans::a350_gl_detail::immediate(item.second,gain);glFinish();auto expected=pixels();
            baseline();bind(font.texture(),0);oans::draw_a350_vertices(item.second,gain);glFinish();require(pixels()==expected,"production pixels differ");++pixel_checks;
            if(item.second.size()>=oans::a350_batch_min_vertices){
                baseline();bind(font.texture(),0);require(oans::a350_gl_detail::batch(item.second,gain),"batch unexpectedly unavailable");glFinish();require(pixels()==expected,"forced batch pixels differ");++batch_checks;
            }
            if(item.second.size()>=oans::a350_batch_min_vertices&&(gain==1||gain==.37)) {
                std::vector<double> timings[2];
                for(int i=0;i<36;++i)for(int method:{i%2,1-i%2}) {
                    baseline();bind(font.texture(),0);glFinish();auto start=Clock::now();
                    if(method)oans::draw_a350_vertices(item.second,gain);else oans::a350_gl_detail::immediate(item.second,gain);
                    double t=ms(start);glFinish();if(i>=6)timings[method].push_back(t);
                }
                out<<argv[3]<<','<<q<<','<<mode<<','<<range<<','<<item.first<<','<<item.second.size()<<','<<gain;
                for(auto& t:timings){std::sort(t.begin(),t.end());out<<','<<std::fixed<<std::setprecision(6)<<t[t.size()/2];}out<<'\n';
            }
            require(glGetError()==GL_NO_ERROR,"GL error");
        }
    }
    font.release();std::cout<<"PASS "<<argv[3]<<": "<<pixel_checks<<" production pixel comparisons, "<<batch_checks<<" forced batch comparisons; driver="<<glGetString(GL_RENDERER)<<'\n';
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
