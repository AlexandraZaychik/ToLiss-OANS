#include "a350_map.hpp"
#include "route_plan.hpp"
#include "map_texture.hpp"
#include "gl_font.hpp"
#include "map_viewport.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#endif

// Offline CPU + real hardware OpenGL rendering path. Time advances deterministically at 60 Hz;
// there is no sleeping, simulator, OpenGL submission or network access.

extern "C" {
__declspec(dllexport) unsigned long NvOptimusEnablement=1;
__declspec(dllexport) int AmdPowerXpressRequestHighPerformance=1;
}
namespace {
void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void allocate(int* n,int count){glGenTextures(count,reinterpret_cast<GLuint*>(n));}
void bind(int n,int){glBindTexture(GL_TEXTURE_2D,n);}
struct Renderer {
    HWND window=nullptr;HDC dc=nullptr;HGLRC context=nullptr;
    GLuint target=0,color=0,query=0;
    using Gen=void(APIENTRY*)(GLsizei,GLuint*);
    using Bind=void(APIENTRY*)(GLenum,GLuint);
    using Attach=void(APIENTRY*)(GLenum,GLenum,GLenum,GLuint,GLint);
    using Status=GLenum(APIENTRY*)(GLenum);
    using Begin=void(APIENTRY*)(GLenum,GLuint);
    using End=void(APIENTRY*)(GLenum);
    using Get64=void(APIENTRY*)(GLuint,GLenum,std::uint64_t*);
    Bind framebuffer=nullptr;Begin begin=nullptr;End end=nullptr;Get64 result=nullptr;
    oans::AtlasFont font;
    Renderer(const std::filesystem::path& assets) {
        SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
        WNDCLASSW c{};c.style=CS_OWNDC;c.lpfnWndProc=DefWindowProcW;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"OANS_HardwareBenchmark";
        require(RegisterClassW(&c)!=0,"Register hidden window failed");
        window=CreateWindowW(c.lpszClassName,L"Offline ND GPU benchmark",WS_POPUP,0,0,600,600,nullptr,nullptr,c.hInstance,nullptr);
        require(window!=nullptr,"Create hidden window failed");dc=GetDC(window);
        PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof p;p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=24;p.cAlphaBits=8;
        const int f=ChoosePixelFormat(dc,&p);require(f&&SetPixelFormat(dc,f,&p),"Set pixel format failed");
        context=wglCreateContext(dc);require(context&&wglMakeCurrent(dc,context),"Create WGL context failed");
        const auto renderer=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
        std::cout<<"GL_VENDOR="<<glGetString(GL_VENDOR)<<"\nGL_RENDERER="<<renderer<<"\nGL_VERSION="<<glGetString(GL_VERSION)<<"\n"<<std::flush;
        require(renderer&&std::string(renderer).find("NVIDIA")!=std::string::npos,"Expected NVIDIA hardware renderer; refusing wrong adapter/software result");
        auto gen=oans::gl_proc<Gen>("glGenFramebuffers");framebuffer=oans::gl_proc<Bind>("glBindFramebuffer");
        auto attach=oans::gl_proc<Attach>("glFramebufferTexture2D");auto status=oans::gl_proc<Status>("glCheckFramebufferStatus");
        auto queries=oans::gl_proc<Gen>("glGenQueries");begin=oans::gl_proc<Begin>("glBeginQuery");end=oans::gl_proc<End>("glEndQuery");result=oans::gl_proc<Get64>("glGetQueryObjectui64v");
        require(gen&&framebuffer&&attach&&status&&queries&&begin&&end&&result,"Hardware FBO/timer query unavailable");
        require(font.load(assets/"font_b612_px2.bin")&&font.upload(allocate,bind),"Font upload failed");
        glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,600,600,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        gen(1,&target);framebuffer(0x8D40,target);attach(0x8D40,0x8CE0,GL_TEXTURE_2D,color,0);glDrawBuffer(0x8CE0);glReadBuffer(0x8CE0);
        require(status(0x8D40)==0x8CD5,"ND target FBO incomplete");queries(1,&query);
        GLint bits=0;auto query_info=oans::gl_proc<void(APIENTRY*)(GLenum,GLenum,GLint*)>("glGetQueryiv");require(query_info!=nullptr,"Query capabilities unavailable");query_info(0x88BF,0x8864,&bits);require(bits>0,"GPU elapsed timer unsupported");
        std::cout<<"ND_target=600x600_RGBA8 map_cache=1024x1024 surface_and_labels timer=GL_TIME_ELAPSED bits="<<bits<<" per_frame_glFinish no_swap no_vsync no_simulator\n"<<std::flush;
    }
    void start() {
        begin(0x88BF,query);
        framebuffer(0x8D40,target);glDrawBuffer(0x8CE0);glReadBuffer(0x8CE0);
        glViewport(0,0,600,600);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,600,0,600,-1,1);
        glMatrixMode(GL_MODELVIEW);glLoadIdentity();glDisable(GL_DEPTH_TEST);glDisable(GL_CULL_FACE);glDisable(GL_SCISSOR_TEST);glDisable(GL_STENCIL_TEST);glDisable(GL_ALPHA_TEST);
        glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);glEnable(GL_TEXTURE_2D);glEnable(GL_BLEND);glBlendFunc(GL_SRC_ALPHA,GL_ONE_MINUS_SRC_ALPHA);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
    }
    void draw(oans::MapTexture& surface,oans::MapTexture& labels,const std::vector<oans::A350Vertex>& base,const std::vector<oans::A350Vertex>& text,const std::vector<oans::A350Vertex>& dynamic,std::uint64_t generation,const oans::A350View& source,const oans::A350View& current) {
        require(surface.update(base,font.texture(),generation,allocate),"Production surface texture update failed");surface.draw(1,bind,source,current);
        require(labels.update(text,font.texture(),generation,allocate,true),"Production label texture update failed");labels.draw(1,bind,source,current);
        bind(font.texture(),0);oans::draw_a350_vertices(dynamic);
    }
    void stop(){end(0x88BF);}
    double gpu_ms(){std::uint64_t ns=0;result(query,0x8866,&ns);return double(ns)/1e6;}
    void validate() {
        require(glGetError()==GL_NO_ERROR,"OpenGL error");std::vector<unsigned char> pixels(600*600*4);glReadPixels(0,0,600,600,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());
        std::size_t lit=0;for(std::size_t i=0;i<pixels.size();i+=4)if(pixels[i]||pixels[i+1]||pixels[i+2])++lit;
        require(lit>100,"Hardware ND rendered empty pixels");std::cout<<"validated_nonblack_pixels="<<lit<<'\n'<<std::flush;
    }
    ~Renderer(){font.release();if(context){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);}if(dc)ReleaseDC(window,dc);if(window)DestroyWindow(window);}
};
}
int main(int argc,char** argv) {
    try {
        if(argc!=11)throw std::runtime_error("Usage: detail_quality_benchmark ASSETS XPLANE_ROOT BUILDINGS OUTPUT_CSV REPEATS ICAO ROUTE UNUSED SPEED_KT FRAME_LIMIT_OR_ZERO");
#ifdef _WIN32
        // Keep all comparisons on one logical processor; never change the host's policy.
        if(!SetThreadAffinityMask(GetCurrentThread(),1))throw std::runtime_error("CPU affinity failed");
#endif
        const double speed=std::stod(argv[9])*oans::kt;
        if(speed<=0)throw std::runtime_error("Speed must be positive");
        constexpr int hz=60;
        const int repeats=std::stoi(argv[5]);
        if(repeats<1)throw std::runtime_error("Repeats must be positive");
        std::vector<oans::IndexEntry> index;
        for(const auto& file:oans::scenery_files(std::filesystem::u8path(argv[2]))) {
            auto part=oans::scan_index(file);index.insert(index.end(),part.begin(),part.end());
        }
        auto entry=oans::find_airport(oans::canonical_index(index),argv[6]);
        if(!entry)throw std::runtime_error("Airport missing");
        const auto airport=oans::load_airport(*entry);
        const auto plan=oans::make_route_plan(airport,argv[7]);
        if(!plan)throw std::runtime_error(plan.error);
        // Round taxi graph corners with tangent quadratic Beziers. The trim
        // distance is <=30 m and <=40% of either adjacent edge; short links
        // never overlap. Resample by arclength below, preserving 20 kt.
        std::vector<oans::Vec2> path{plan.points.front()};
        int rounded=0;
        for(size_t i=1;i+1<plan.points.size();++i) {
            const auto p=plan.points[i],incoming=p-plan.points[i-1],outgoing=plan.points[i+1]-p;
            const double a=oans::length(incoming),b=oans::length(outgoing);
            if(a<1e-6||b<1e-6)continue;
            const auto u=incoming*(1/a),v=outgoing*(1/b);
            if(oans::dot(u,v)>.9999){path.push_back(p);continue;}
            const double trim=std::min({30.,a*.4,b*.4});
            const auto entry=p-u*trim,exit=p+v*trim;path.push_back(entry);
            for(int j=1;j<=48;++j){const double t=j/48.;path.push_back(entry*((1-t)*(1-t))+p*(2*(1-t)*t)+exit*(t*t));}
            ++rounded;
        }
        path.push_back(plan.points.back());
        std::vector<double> stations{0};
        for(size_t i=1;i<path.size();++i)stations.push_back(stations.back()+oans::length(path[i]-path[i-1]));
        const auto from=path.front(),to=path.back();
        const double distance=stations.back(),seconds=distance/speed;
        const int full_frames=int(std::ceil(seconds*hz));
        const int frame_limit=std::stoi(argv[10]);
        const int frames=frame_limit>0?std::min(frame_limit,full_frames):full_frames;
        const auto first_axis=path[1]-path[0];
        const double heading=std::fmod(std::atan2(first_axis.x,first_axis.y)*180/oans::pi+360,360);
        auto geo=[&](oans::Vec2 p) {
            return oans::Geo{airport.lat+p.y/oans::earth*180/oans::pi,
                airport.lon+p.x/(oans::earth*std::cos(airport.lat*oans::pi/180))*180/oans::pi};
        };
        const auto start=geo(from),end=geo(to);
        std::cout<<std::setprecision(12)<<"apt="<<entry->file.u8string()<<"\n"
            <<"buildings="<<argv[3]<<"\nroute=rounded_taxi_path "<<argv[6]<<" "<<argv[7]<<" raw_points="<<plan.points.size()<<" raw_metres="<<plan.metres<<" rounded_corners="<<rounded<<" path_points="<<path.size()
            <<" heading_deg="<<heading<<" distance_m="<<distance<<"\n"
            <<"start_lat="<<start.lat<<" start_lon="<<start.lon<<" end_lat="<<end.lat<<" end_lon="<<end.lon<<"\n"
            <<"mode=ARC sides=1 speed_kt="<<argv[9]<<" seconds="<<seconds<<" hz="<<hz<<" frames="<<frames<<" repeats="<<repeats
            <<" CPU_affinity=logical_0 real_hardware_GL no_simulator no_traffic no_route_overlay\n"<<std::flush;
        Renderer renderer(std::filesystem::u8path(argv[1]));
        const auto load_start=std::chrono::steady_clock::now();
        oans::A350Map map(std::filesystem::u8path(argv[1]),*entry,std::filesystem::u8path(argv[3]));
        const auto stats=map.stats();
        std::cout<<"load_ms="<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-load_start).count()
            <<" pavements="<<stats.pavements<<" stand_labels="<<stats.stand_labels
            <<" terminals="<<stats.terminals<<" buildings="<<stats.buildings<<"\n"<<std::flush;
        std::ofstream out(std::filesystem::u8path(argv[4]));out.exceptions(std::ios::failbit|std::ios::badbit);
        out<<"round,quality,range_nm,frames,total_ms,mean_frame_ms,p50_frame_ms,p95_frame_ms,base_builds,base_hits,base_build_total_ms,mean_base_build_ms,mean_base_vertices,peak_base_vertices,mean_dynamic_vertices,cpu_geometry_mean_ms,cpu_submit_mean_ms,gpu_elapsed_mean_ms,gpu_elapsed_p95_ms,completion_wait_mean_ms\n";
        {
            std::ofstream route(std::filesystem::u8path(std::string(argv[4])+".route.csv"));
            route<<"station_m,x_m,y_m,lat,lon\n"<<std::setprecision(12);
            for(size_t i=0;i<path.size();++i){const auto g=geo(path[i]);route<<stations[i]<<','<<path[i].x<<','<<path[i].y<<','<<g.lat<<','<<g.lon<<'\n';}
        }
        const char* names[]={"ultra","max","xhigh","high","medium","low"};
        for(int round=0;round<repeats;++round)for(double range:{2.,1.,.2,.1})for(int slot=0;slot<6;++slot) {
            // Rotate/reverse order to reduce temperature and background-load bias.
            const int quality=round%2?(round+5-slot)%6:(round+slot)%6;
            oans::A350View view;view.mode=3;view.range=range;view.detail_quality=quality;
            view.psi=view.mag=heading;view.angle=heading*oans::pi/180;
            const auto vp=oans::map_viewport(3,false);
            view.anchor=vp.anchor;view.radius=vp.radius;view.bottom=vp.bottom;
            view.scale=vp.radius/(range*1852);
            auto pose=[&](int frame) {
                const double d=std::min(distance,speed*frame/hz);
                size_t i=std::min(size_t(std::upper_bound(stations.begin(),stations.end(),d)-stations.begin()),path.size()-1);
                if(i<1)i=1;
                const auto delta=path[i]-path[i-1];
                const double len=stations[i]-stations[i-1];
                const auto position=geo(path[i-1]+delta*(len>0?(d-stations[i-1])/len:0));
                view.psi=view.mag=std::fmod(std::atan2(delta.x,delta.y)*180/oans::pi+360,360);
                view.angle=view.psi*oans::pi/180;
                view.lat=position.lat;view.lon=position.lon;
                view.center=oans::local(view.lat,view.lon,map.ref_lat(),map.ref_lon());
            };

            oans::MapTexture surface,label_texture;
            auto draw_frame=[&](int frame,bool force,std::vector<double>* total,std::vector<double>* cpu,std::vector<double>* submit,std::vector<double>* gpu,std::vector<double>* wait,std::uint64_t& bv,std::uint64_t& dv,std::uint64_t& peak) {
                pose(frame);const auto t0=std::chrono::steady_clock::now();
                const auto& base=map.render_base(0,view,frame/double(hz),force,frame);
                const auto& text=map.base_labels(0);
                const auto& dynamic=map.render_dynamic(view);
                const auto t1=std::chrono::steady_clock::now();
                renderer.start();renderer.draw(surface,label_texture,base,text,dynamic,map.cache_stats(0).builds,map.base_view(0),view);renderer.stop();
                const auto t2=std::chrono::steady_clock::now();glFinish();const auto t3=std::chrono::steady_clock::now();
                const double gpu_time=renderer.gpu_ms();
                require(glGetError()==GL_NO_ERROR,"GL error in measured frame");
                if(total) {
                    auto ms=[](auto a,auto b){return std::chrono::duration<double,std::milli>(b-a).count();};
                    total->push_back(ms(t0,t3));cpu->push_back(ms(t0,t1));submit->push_back(ms(t1,t2));gpu->push_back(gpu_time);wait->push_back(ms(t2,t3));
                    bv+=base.size()+text.size();dv+=dynamic.size();peak=std::max(peak,std::uint64_t(base.size()+text.size()));
                }
            };
            std::uint64_t base_vertices=0,dynamic_vertices=0,peak_vertices=0;
            for(int frame=0;frame<120;++frame)draw_frame(frame,false,nullptr,nullptr,nullptr,nullptr,nullptr,base_vertices,dynamic_vertices,peak_vertices);
            draw_frame(0,true,nullptr,nullptr,nullptr,nullptr,nullptr,base_vertices,dynamic_vertices,peak_vertices);
            const auto before=map.cache_stats(0);
            std::vector<double> times,cpu_times,submit_times,gpu_times,wait_times;
            for(auto* v:{&times,&cpu_times,&submit_times,&gpu_times,&wait_times})v->reserve(frames);
            for(int frame=1;frame<=frames;++frame)draw_frame(frame,false,&times,&cpu_times,&submit_times,&gpu_times,&wait_times,base_vertices,dynamic_vertices,peak_vertices);
            const auto after=map.cache_stats(0);
            const auto builds=after.builds-before.builds,hits=after.hits-before.hits;
            const double total=std::accumulate(times.begin(),times.end(),0.),build_ms=after.total_ms-before.total_ms;
            std::sort(times.begin(),times.end());
            out<<std::fixed<<std::setprecision(8)<<round+1<<','<<names[quality]<<','<<range<<','<<frames<<','
                <<total<<','<<total/frames<<','<<times[frames/2]<<','<<times[(frames-1)*95/100]<<','
                <<builds<<','<<hits<<','<<build_ms<<','<<(builds?build_ms/builds:0)<<','
                <<double(base_vertices)/frames<<','<<peak_vertices<<','<<double(dynamic_vertices)/frames;
            auto mean=[&](const auto& v){return std::accumulate(v.begin(),v.end(),0.)/frames;};
            std::sort(gpu_times.begin(),gpu_times.end());
            out<<','<<mean(cpu_times)<<','<<mean(submit_times)<<','<<mean(gpu_times)<<','<<gpu_times[(frames-1)*95/100]<<','<<mean(wait_times)<<'\n'<<std::flush;
            renderer.validate();surface.release();label_texture.release();
            std::cout<<"round="<<round+1<<" range="<<range<<" quality="<<names[quality]
                <<" gpu_ms="<<mean(gpu_times)<<" cpu_geometry_ms="<<mean(cpu_times)<<" mean_ms="<<total/frames<<" builds="<<builds<<" mean_vertices="<<base_vertices/frames<<'\n'<<std::flush;
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
