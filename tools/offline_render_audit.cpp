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

#include <cstddef>
#include <numeric>
#include <cstdint>
#include <functional>
void batch(const std::vector<oans::A350Vertex>& v) {
    if(v.empty())return;
    GLint env=0,buffer=0;glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&env);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
    auto bind_buffer=oans::gl_proc<void(APIENTRY*)(GLenum,GLuint)>("glBindBuffer");
    glGetIntegerv(0x8894,&buffer);glPushClientAttrib(GL_CLIENT_VERTEX_ARRAY_BIT);
    if(bind_buffer)bind_buffer(0x8892,0);
    glEnableClientState(GL_VERTEX_ARRAY);glEnableClientState(GL_COLOR_ARRAY);glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    const auto* p=reinterpret_cast<const unsigned char*>(v.data());
    glVertexPointer(2,GL_FLOAT,sizeof(oans::A350Vertex),p+offsetof(oans::A350Vertex,x));
    glTexCoordPointer(2,GL_FLOAT,sizeof(oans::A350Vertex),p+offsetof(oans::A350Vertex,u));
    glColorPointer(4,GL_UNSIGNED_BYTE,sizeof(oans::A350Vertex),p+offsetof(oans::A350Vertex,rgba));
    glDrawArrays(GL_TRIANGLES,0,static_cast<GLsizei>(v.size()));
    glPopClientAttrib();if(bind_buffer)bind_buffer(0x8892,buffer);
    glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,env);
}
using Clock=std::chrono::steady_clock;
double elapsed(Clock::time_point t){return std::chrono::duration<double,std::milli>(Clock::now()-t).count();}
uint64_t hash(const std::vector<oans::A350Vertex>& v) {
    uint64_t h=1469598103934665603ull;
    for(const auto& x:v)for(auto b:std::array<uint32_t,5>{uint32_t(std::hash<float>{}(x.x)),uint32_t(std::hash<float>{}(x.y)),uint32_t(std::hash<float>{}(x.u)),uint32_t(std::hash<float>{}(x.v)),x.rgba}){h^=b;h*=1099511628211ull;}
    return h;
}
struct Timing {double mean,p50,p95;};
Timing measure(const std::function<void(int)>& f,int n) {
    for(int i=0;i<10;++i)f(i);
    std::vector<double> t;t.reserve(n);
    for(int i=0;i<n;++i){auto start=Clock::now();f(i);t.push_back(elapsed(start));}
    double sum=std::accumulate(t.begin(),t.end(),0.);std::sort(t.begin(),t.end());
    return {sum/n,t[n/2],t[(n-1)*95/100]};
}
int main(int argc,char** argv) {
 try {
    require(argc==7,"ASSETS XPLANE ICAO BUILDINGS OUTPUT_DIR VARIANT");
    SetThreadAffinityMask(GetCurrentThread(),1);
    auto assets=std::filesystem::u8path(argv[1]),out=std::filesystem::u8path(argv[5]);
    std::filesystem::create_directories(out);
    std::vector<oans::IndexEntry> index;
    for(auto& f:oans::scenery_files(std::filesystem::u8path(argv[2]))){auto e=oans::scan_index(f);index.insert(index.end(),e.begin(),e.end());}
    auto e=oans::find_airport(oans::canonical_index(index),argv[3]);require(bool(e),"airport missing");
    const auto apt=oans::load_airport(*e);
    oans::A350Map map(assets,*e,std::filesystem::u8path(argv[4]));auto stats=map.stats();
    Context context;oans::AtlasFont font;require(font.load(assets/"font_b612_px2.bin")&&font.upload(allocate,bind),"font");
    std::cout<<"META,"<<argv[3]<<','<<argv[6]<<','<<glGetString(GL_RENDERER)<<','<<glGetString(GL_VERSION)<<','<<e->file.u8string()<<','<<stats.pavements<<','<<stats.stand_labels<<','<<stats.terminals<<','<<stats.buildings<<'\n';
    std::cout<<"kind,airport,variant,quality,range,traffic,layer,vertices,mean_ms,p50_ms,p95_ms,hash\n";
    // Shared realistic taxiway pose, not an empty airport-reference corner.
    oans::Vec2 pose=oans::local(apt.reference.lat,apt.reference.lon,apt.lat,apt.lon);double best=1e100,heading=0;
    for(const auto& edge:apt.edges)if(!edge.runway&&apt.nodes.count(edge.a)&&apt.nodes.count(edge.b)){
        auto a=apt.nodes.at(edge.a),b=apt.nodes.at(edge.b);double score=oans::length((a+b)*.5-pose);
        if(score<best&&oans::length(b-a)>30){best=score;heading=std::atan2((b-a).x,(b-a).y)*180/oans::pi;}
    }
    if(best<1e99)for(const auto& edge:apt.edges)if(!edge.runway&&apt.nodes.count(edge.a)&&apt.nodes.count(edge.b)){
        auto a=apt.nodes.at(edge.a),b=apt.nodes.at(edge.b);
        if(oans::length(b-a)>30&&std::abs(oans::length((a+b)*.5-pose)-best)<1e-6){pose=(a+b)*.5;break;}
    }
    auto emit=[&](const char* kind,int q,double range,int traffic,const char* layer,size_t vertices,Timing t,uint64_t h){
        std::cout<<kind<<','<<argv[3]<<','<<argv[6]<<','<<q<<','<<range<<','<<traffic<<','<<layer<<','<<vertices<<','<<std::fixed<<std::setprecision(6)<<t.mean<<','<<t.p50<<','<<t.p95<<','<<h<<std::endl;};
    for(int q:{4,0})for(double range:{.2,.5,2.})for(int traffic:{0,32}) {
        oans::A350View v;v.mode=3;v.range=range;v.detail_quality=q;
        v.lat=apt.lat+pose.y/oans::earth*180/oans::pi;v.lon=apt.lon+pose.x/(oans::earth*std::cos(apt.lat*oans::pi/180))*180/oans::pi;
        auto vp=oans::map_viewport(3,false);v.anchor=vp.anchor;v.radius=vp.radius;v.bottom=vp.bottom;v.scale=vp.radius/(range*1852);
        v.center=oans::local(v.lat,v.lon,map.ref_lat(),map.ref_lon());v.psi=v.mag=heading;v.angle=heading*oans::pi/180;
        v.fms_airport=e->icao;v.fms_runway=apt.runways.front().name;
        for(int i=0;i<traffic;++i){v.traffic.push_back({i+1,v.lat+(i/8-2)*.00012,v.lon+(i%8-4)*.00012,heading+i*10,"TEST"+std::to_string(i),"A320",15});
            v.crosses.push_back(v.center+oans::Vec2{double(i*12),double(i*7)});v.flags.push_back(v.center+oans::Vec2{double(-i*10),double(i*8)});}
        auto moving=[&](int i){auto x=v;x.center.y+=i*.13;x.lat+=i*.13/oans::earth*180/oans::pi;x.mag+=i*.05;x.psi+=i*.05;x.angle=x.psi*oans::pi/180;return x;};
        auto base=map.render_base(0,v,0,true);auto labels=map.base_labels(0);
        for(int layer:{2,3,4}) {
            auto render=[&](const oans::A350View& x)->const std::vector<oans::A350Vertex>& {return layer==2?map.render_dynamic(x):layer==3?map.render_compass(x):map.render_aircraft(x);};
            auto vertices=render(v);auto h=hash(vertices);volatile size_t sink=0;
            auto t=measure([&](int i){sink+=render(moving(i)).size();},300);
            emit("cpu_dynamic",q,range,traffic,layer==2?"combined":layer==3?"compass":"aircraft",vertices.size(),t,h);
        }
        emit("cpu_base_rebuild",q,range,traffic,"surface_labels",base.size()+labels.size(),measure([&](int i){map.render_base(0,moving(i),10+i,true);},30),0);
        // Moving two NDs, retaining production 10 Hz motion policy. No GPU here.
        emit("cpu_dual_frame",q,range,traffic,"normal",0,measure([&](int i){auto x=moving(i);for(int side=0;side<2;++side){map.render_base(side,x,i/60.,false,i);map.render_dynamic(x);}},600),0);
        emit("cpu_dual_frame",q,range,traffic,"route_split",0,measure([&](int i){auto x=moving(i);for(int side=0;side<2;++side){map.render_base(side,x,i/60.,false,i);map.render_compass(x);map.render_aircraft(x);}},600),0);
        base=map.render_base(0,v,0,true);labels=map.base_labels(0);auto dynamic=map.render_dynamic(v);
        std::vector<std::pair<const char*,std::vector<oans::A350Vertex>>> buffers={{"surface",base},{"labels",labels},{"dynamic",dynamic}};
        if(std::string(argv[6])=="baseline")for(auto& item:buffers) {
            baseline();bind(font.texture(),0);oans::a350_gl_detail::immediate(item.second,1);glFinish();auto a=pixels();
            baseline();bind(font.texture(),0);batch(item.second);glFinish();auto b=pixels();
            require(a==b,"client arrays changed pixels");
            for(int method=0;method<2;++method){
                auto draw=[&]{if(method)batch(item.second);else oans::a350_gl_detail::immediate(item.second,1);};
                auto submit=measure([&](int){baseline();bind(font.texture(),0);glFinish();draw();glFinish();},40);
                emit(method?"gl_batch_finish":"gl_immediate_finish",q,range,traffic,item.first,item.second.size(),submit,hash(item.second));
                std::vector<double> samples;
                for(int i=0;i<50;++i){baseline();bind(font.texture(),0);glFinish();auto start=Clock::now();draw();samples.push_back(elapsed(start));glFinish();}
                const double sum=std::accumulate(samples.begin(),samples.end(),0.);std::sort(samples.begin(),samples.end());
                emit(method?"gl_batch_cpu":"gl_immediate_cpu",q,range,traffic,item.first,item.second.size(),{sum/samples.size(),samples[25],samples[46]},hash(item.second));

            }
        }
        baseline();bind(font.texture(),0);oans::draw_a350_vertices(base);oans::draw_a350_vertices(labels);oans::draw_a350_vertices(dynamic);glFinish();auto preview=pixels();
        if(q==4)bmp(out/(std::string(argv[3])+"-"+argv[6]+"-"+std::to_string(range)+"-"+std::to_string(traffic)+".bmp"),preview);
        require(glGetError()==GL_NO_ERROR,"GL error");
    }
    font.release();std::cout<<"PASS completed offline measurements; immediate/client-array pixel comparisons apply to baseline variant only\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
