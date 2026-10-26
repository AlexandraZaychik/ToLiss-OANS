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
int main(int argc,char** argv) {
    try {
        require(argc>=5,"Usage: map_texture_probe ASSETS XPLANE_ROOT ICAO BUILDINGS [FRAMES=300]");
        int frames=argc>5?std::stoi(argv[5]):300;require(frames>=30,"frames >= 30");
        std::vector<oans::IndexEntry> index;
        for(const auto& file:oans::scenery_files(std::filesystem::u8path(argv[2]))) {
            auto entries=oans::scan_index(file);index.insert(index.end(),entries.begin(),entries.end());
        }
        auto entry=oans::find_airport(oans::canonical_index(index),argv[3]);require(bool(entry),"airport absent");
        oans::A350Map map(std::filesystem::u8path(argv[1]),*entry,std::filesystem::u8path(argv[4]));
        oans::A350View base;base.mode=3;base.range=.5;base.psi=base.mag=250.77468872;base.angle=base.psi*oans::pi/180;
        base.lat=entry->icao=="ZUCK"?29.716054916:map.ref_lat();base.lon=entry->icao=="ZUCK"?106.651611328:map.ref_lon();
        auto vp=oans::map_viewport(3,false);base.radius=vp.radius;base.bottom=vp.bottom;base.anchor=vp.anchor;
        base.scale=vp.radius/(base.range*1852);base.center=oans::local(base.lat,base.lon,map.ref_lat(),map.ref_lon());
        const auto original=map.render(0,base),ground=map.render_base(0,base,0),moving=map.render_dynamic(base);
        require(!ground.empty()&&original.size()>=moving.size(),"empty base/dynamic geometry");
        bool offscreen=false;
        for(const auto& vertex:ground)if(vertex.x < -32 || vertex.x > 632 || vertex.y < -32 || vertex.y > 632)offscreen=true;
        require(offscreen,"base geometry must populate the offscreen guard band");
        for(size_t i=0;i<moving.size();++i) {
            const auto& a=original[original.size()-moving.size()+i];const auto& b=moving[i];
            require(a.x==b.x&&a.y==b.y&&a.u==b.u&&a.v==b.v&&a.rgba==b.rgba,"dynamic geometry differs");
        }
        auto before=map.cache_stats(0).builds;auto pose=base;pose.center.y+=2/base.scale;
        map.render_base(0,pose,.05);require(map.cache_stats(0).builds==before,"10 Hz limit");
        map.render_base(0,pose,.1);require(map.cache_stats(0).builds==before+1,"motion refresh due at 100ms");
        auto traffic=pose;traffic.traffic={{1,pose.lat,pose.lon,80}};traffic.psi+=20;traffic.mag+=20;
        map.render_base(0,traffic,.21);require(map.cache_stats(0).builds==before+1,"dynamic input rebuilt base");
        auto zoom=pose;zoom.scale*=2;map.render_base(0,zoom,.22);require(map.cache_stats(0).builds==before+2,"zoom must refresh immediately");
        auto pan=zoom;pan.center.x+=2/pan.scale;map.render_base(0,pan,.23,true);require(map.cache_stats(0).builds==before+3,"interactive pan must refresh immediately");
        pan.runway_ahead={{"02","20",0,0}};pan.alert_blink=true;map.render_base(0,pan,.24);require(map.cache_stats(0).builds==before+4,"alert must refresh immediately");
        map.render_base(1,base,0);auto threshold_builds=map.cache_stats(1).builds;
        auto small=base;small.center.x+=.9/base.scale;map.render_base(1,small,1);
        require(map.cache_stats(1).builds==threshold_builds,"subpixel motion should reuse base after timer expires");
        small.center.x+=.2/base.scale;map.render_base(1,small,1.1);
        require(map.cache_stats(1).builds==threshold_builds+1,"cumulative 1px motion should rebuild base");
        auto source=base;source.angle=359*oans::pi/180;auto wrapped=source;wrapped.angle=-oans::pi/180;
        for(auto p:std::array<oans::Vec2,3>{{{0,0},{300,300},{600,600}}})
            require(oans::length(oans::map_cached_point(p,source,wrapped)-p)<1e-8,"360-degree reprojection seam");
        oans::A350View north;north.anchor={300,300};auto east=north;east.angle=oans::pi/2;
        require(oans::length(oans::map_cached_point({400,300},north,east)-oans::Vec2{300,200})<1e-8,"reprojection rotation sign");
        auto turn=base;turn.angle+=4*oans::pi/180;turn.center.x+=2/base.scale;
        require(oans::map_texture_covers(base,turn),"turn overscan coverage");
        map.render_base(2,base,0);auto turn_builds=map.cache_stats(2).builds;
        map.render_base(2,turn,.04);require(map.cache_stats(2).builds==turn_builds,"small turn must reproject without rebuild");
        auto jump=turn;jump.angle+=oans::pi/2;
        require(!oans::map_texture_covers(base,jump),"large jump should exhaust coverage");
        map.render_base(2,jump,.05);require(map.cache_stats(2).builds==turn_builds+1,"large jump must rebuild immediately");
        for(int side=0;side<2;++side)map.render_base(side,base,0);
        before=map.cache_stats(0).builds;auto right_before=map.cache_stats(1).builds;
        map.render_base(0,turn,.1,false,100);map.render_base(1,turn,.1005,false,100);
        require(map.cache_stats(0).builds==before+1&&map.cache_stats(1).builds==right_before,"dual ND rebuilds should stagger");
        map.render_base(1,turn,.101,false,100);
        require(map.cache_stats(1).builds==right_before,"second callback in same frame must remain deferred");
        map.render_base(1,turn,.1+1./30,false,101);require(map.cache_stats(1).builds==right_before+1,"deferred ND must rebuild next frame");
        map.render_base(2,base,-1);require(map.base_view(2).angle==base.angle,"clock rewind must rebuild");
        std::cout<<"PASS 10Hz, 1px, dynamic isolation, immediate zoom/pan/alert, reprojection wrap/coverage, dual ND staggering, clock rewind\n";
        Context context;oans::AtlasFont font;require(font.load(std::filesystem::u8path(argv[1])/"font_b612_px2.bin")&&font.upload(allocate,bind),"font upload");
        std::cout<<"GL="<<glGetString(GL_VERSION)<<" renderer="<<glGetString(GL_RENDERER)<<"; Release O2; 30Hz poses,15kt,0.5NM,ARC\n";
        oans::MapTexture texture;baseline();bind(font.texture(),0);oans::draw_a350_vertices(original);glFinish();auto direct=pixels();
        baseline();glEnable(GL_SCISSOR_TEST);glScissor(3,5,590,580);
        GLint viewport[4],scissor[4],fbo=0;glGetIntegerv(GL_VIEWPORT,viewport);glGetIntegerv(GL_SCISSOR_BOX,scissor);glGetIntegerv(0x8CA6,&fbo);
        require(texture.update(ground,font.texture(),1,allocate),"FBO unavailable/incomplete");
        GLint check[4];glGetIntegerv(GL_VIEWPORT,check);for(int i=0;i<4;++i)require(check[i]==viewport[i],"viewport leak");
        glGetIntegerv(GL_SCISSOR_BOX,check);for(int i=0;i<4;++i)require(check[i]==scissor[i],"scissor leak");
        glGetIntegerv(0x8CA6,check);require(check[0]==fbo&&glIsEnabled(GL_SCISSOR_TEST),"FBO/enable leak");
        baseline();texture.draw(1,bind,base,base);bind(font.texture(),0);oans::draw_a350_vertices(moving);glFinish();auto cached=pixels();
        double error=0;size_t lit=0;for(size_t i=0;i<direct.size();++i){error+=std::abs(int(direct[i])-int(cached[i]));if(cached[i])++lit;}
        error/=direct.size();require(lit>1000&&error<5,"cached image differs excessively");
        std::filesystem::create_directories("reports/runtime/map-texture");bmp("reports/runtime/map-texture/direct.bmp",direct);bmp("reports/runtime/map-texture/cached.bmp",cached);
        std::cout<<"PASS FBO/viewport/scissor restoration and nonempty composite; mean RGB error="<<error<<"/255 (padded 1024 texture resampling)\n";
        baseline();bind(font.texture(),0);oans::draw_a350_vertices(map.render(2,turn));glFinish();auto turn_direct=pixels();
        baseline();texture.draw(1,bind,base,turn);bind(font.texture(),0);oans::draw_a350_vertices(map.render_dynamic(turn));glFinish();auto turn_cached=pixels();
        error=0;for(size_t i=0;i<turn_direct.size();++i)error+=std::abs(int(turn_direct[i])-int(turn_cached[i]));
        error/=turn_direct.size();require(error<8,"reprojected turn image differs excessively");
        require(turn_cached!=cached,"cached image must move between base rebuilds");
        bmp("reports/runtime/map-texture/turn-direct.bmp",turn_direct);bmp("reports/runtime/map-texture/turn-reprojected.bmp",turn_cached);
        baseline();bind(font.texture(),0);oans::MapTexture::draw_fallback(ground,1,base,turn);oans::draw_a350_vertices(map.render_dynamic(turn));
        glFinish();auto fallback=pixels();double fallback_error=0;
        for(size_t i=0;i<fallback.size();++i)fallback_error+=std::abs(int(fallback[i])-int(turn_cached[i]));
        require(fallback_error/fallback.size()<5,"FBO fallback reprojection differs");
        std::cout<<"PASS per-frame turn reprojection + fallback; mean turn RGB error="<<error<<"/255\n";texture.release();
        for(int mode:{2,3,4})for(bool panel:{false,true}) {
            auto v=base;v.mode=mode;auto viewport=oans::map_viewport(mode,panel);
            v.anchor=viewport.anchor;v.radius=viewport.radius;v.bottom=viewport.bottom;v.scale=v.radius/(v.range*1852);
            baseline();bind(font.texture(),0);oans::draw_a350_vertices(map.render(2,v));glFinish();auto expected=pixels();
            const auto& g=map.render_base(2,v,0);
            require(texture.update(g,font.texture(),map.cache_stats(2).builds,allocate),"mode/panel FBO");
            baseline();texture.draw(1,bind,v,v);bind(font.texture(),0);oans::draw_a350_vertices(map.render_dynamic(v));glFinish();auto actual=pixels();
            double difference=0;for(size_t i=0;i<expected.size();++i)difference+=std::abs(int(expected[i])-int(actual[i]));
            require(difference/expected.size()<5,"mode/panel fresh cached image differs");
            std::cout<<"PASS cached mode="<<mode<<" panel="<<panel<<" mean RGB error="<<difference/expected.size()<<"/255\n";
        }
        texture.release();
        std::cout<<"path,turn_deg_s,sides,mean_ms,p50_ms,p95_ms,max_ms,base_builds,base_hits,double_build_frames\n";
        for(double turn_rate:{0.,6.,30.})for(int sides:{1,2})for(bool optimized:{false,true}) {
            std::array<oans::MapTexture,2> textures;
            for(int side=0;side<sides;++side) {
                baseline();const auto& v=optimized?map.render_base(side,base,0):map.render(side,base);
                if(optimized)require(textures[side].update(v,font.texture(),map.cache_stats(side).builds,allocate),"warmup texture");
            }
            std::uint64_t builds0=0,hits0=0;for(int side=0;side<sides;++side){builds0+=map.cache_stats(side).builds;hits0+=map.cache_stats(side).hits;}
            std::vector<double> times;int double_build_frames=0;
            for(int frame=1;frame<=frames;++frame) {
                auto view=base;const double distance=15*oans::kt*frame/30.;
                view.center.y+=distance;view.lat+=distance/oans::earth*180/oans::pi;
                view.psi+=turn_rate*frame/30.;view.mag+=turn_rate*frame/30.;view.angle=view.psi*oans::pi/180;
                glFinish();auto start=std::chrono::steady_clock::now();
                int built_this_frame=0;
                for(int side=0;side<sides;++side) {
                    baseline();bind(font.texture(),0);
                    const auto build_count=map.cache_stats(side).builds;
                    if(optimized) {
                        const auto& v=map.render_base(side,view,frame/30.,false,frame);
                        require(textures[side].update(v,font.texture(),map.cache_stats(side).builds,allocate),"texture update");
                        textures[side].draw(1,bind,map.base_view(side),view);
                        bind(font.texture(),0);oans::draw_a350_vertices(map.render_dynamic(view));
                    } else oans::draw_a350_vertices(map.render(side,view));
                    if(map.cache_stats(side).builds>build_count)++built_this_frame;
                }
                if(built_this_frame>1)++double_build_frames;
                glFinish();times.push_back(std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
            }
            std::uint64_t builds=0,hits=0;for(int side=0;side<sides;++side){builds+=map.cache_stats(side).builds;hits+=map.cache_stats(side).hits;}
            double sum=0;for(auto t:times)sum+=t;std::sort(times.begin(),times.end());
            std::cout<<(optimized?"reproject_10Hz":"original_0.05px")<<','<<turn_rate<<','<<sides<<','<<std::fixed<<std::setprecision(4)<<sum/frames<<','<<times[frames/2]<<','<<times[(frames-1)*95/100]<<','<<times.back()<<','<<builds-builds0<<','<<hits-hits0<<','<<double_build_frames<<std::endl;
            if(optimized&&sides==2)require(double_build_frames==0,"motion rebuilds must not coincide at 30FPS");
            for(auto& t:textures)t.release();
        }
        require(glGetError()==GL_NO_ERROR,"OpenGL error");font.release();
    }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
}
