// Offline WGL preview of the ACTUAL plugin draw_map/render_rect functions.
// The simulator API is supplied by the GL-enabled test SDK; no simulator UI.
#include "../src/plugin.cpp"
#include <iostream>
namespace {
struct PreviewGL {
    HWND w=nullptr;HDC dc=nullptr;HGLRC context=nullptr;
    PreviewGL() {
        WNDCLASSW c{};c.style=CS_OWNDC;c.lpfnWndProc=DefWindowProcW;c.hInstance=GetModuleHandleW(nullptr);c.lpszClassName=L"OansRouteNDPreview";
        RegisterClassW(&c);w=CreateWindowW(c.lpszClassName,L"Offline ND",WS_POPUP,0,0,1200,1200,nullptr,nullptr,c.hInstance,nullptr);
        dc=GetDC(w);PIXELFORMATDESCRIPTOR p{};p.nSize=sizeof p;p.nVersion=1;p.dwFlags=PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL;p.iPixelType=PFD_TYPE_RGBA;p.cColorBits=24;p.cAlphaBits=8;
        int format=ChoosePixelFormat(dc,&p);if(!format||!SetPixelFormat(dc,format,&p))throw std::runtime_error("Pixel format");
        context=wglCreateContext(dc);if(!context||!wglMakeCurrent(dc,context))throw std::runtime_error("WGL context");
    }
    ~PreviewGL(){if(context){wglMakeCurrent(nullptr,nullptr);wglDeleteContext(context);}if(dc)ReleaseDC(w,dc);if(w)DestroyWindow(w);}
};
void write_bmp(const std::filesystem::path& path) {
    const int size=1200;std::vector<unsigned char> rgb(size*size*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,size,size,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
    for(size_t i=0;i<rgb.size();i+=3)std::swap(rgb[i],rgb[i+2]);
    BITMAPFILEHEADER h{};h.bfType=0x4d42;h.bfOffBits=sizeof h+sizeof(BITMAPINFOHEADER);h.bfSize=h.bfOffBits+DWORD(rgb.size());
    BITMAPINFOHEADER info{};info.biSize=sizeof info;info.biWidth=size;info.biHeight=size;info.biPlanes=1;info.biBitCount=24;info.biSizeImage=DWORD(rgb.size());
    std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&h),sizeof h);out.write(reinterpret_cast<char*>(&info),sizeof info);out.write(reinterpret_cast<char*>(rgb.data()),rgb.size());
}
}
int main(int argc,char** argv) {
    if(argc!=5){std::cerr<<"Usage: route_nd_preview PROJECT_ROOT XPLANE_ROOT ROUTE OUTPUT.bmp\n";return 2;}
    try {
        PreviewGL gl;plugin_dir=std::filesystem::u8path(argv[1]);sim_dir=std::filesystem::u8path(argv[2]);
        const auto parsed=parse_route_input(argv[3]);if(parsed.icao.empty()||!parsed.error.empty())throw std::runtime_error("Include [ICAO]");
        std::vector<IndexEntry> index;
        for(const auto& f:scenery_files(sim_dir)){auto entries=scan_index(f);index.insert(index.end(),entries.begin(),entries.end());}
        auto entry=find_airport(canonical_index(index),parsed.icao);if(!entry)throw std::runtime_error("Airport absent");
        airport=std::make_shared<Airport>(load_airport(*entry));
        a350_map=std::make_shared<A350Map>(plugin_dir/"assets",*entry,plugin_dir/"cache"/("osm2_"+parsed.icao+".json"));
        a350_ui=std::make_unique<A350Map>(plugin_dir/"assets");
        if(!map_font.load(plugin_dir/"assets/font_b612_px2.bin"))throw std::runtime_error("Font absent");
        route_plan=make_route_plan(*airport,argv[3]);if(!route_plan)throw std::runtime_error(route_plan.error);
        route_quads=route_ribbon(route_plan.points);airport_map_loading=false;enabled=true;
        Vec2 lo=route_plan.points.front(),hi=lo;
        for(auto p:route_plan.points){lo.x=std::min(lo.x,p.x);lo.y=std::min(lo.y,p.y);hi.x=std::max(hi.x,p.x);hi.y=std::max(hi.y,p.y);}
        auto& view=views[0];view.follow=false;view.center=(lo+hi)*.5;view.orientation=0;
        double radius=0;for(auto p:route_plan.points)radius=std::max(radius,length(p-view.center));
        const double required=radius*1.22/1852;
        view.range_nm=5;for(double range:{.2,.5,1.,2.,5.})if(range>=required){view.range_nm=range;break;}
        view.panel=Panel::closed;
        bind_refs();if(!aircraft_backend)throw std::runtime_error("Preview aircraft backend absent");
        XPLMSetDatai(dr("mode0").id,4);nd_on[0]=true;
        // Put a simulated ownship on the route to verify it remains above the ribbon.
        const auto own=route_plan.points[route_plan.points.size()/2];
        XPLMSetDatad(dr("lat").id,airport->lat+own.y/earth*180/pi);
        XPLMSetDatad(dr("lon").id,airport->lon+own.x/(earth*std::cos(airport->lat*pi/180))*180/pi);
        XPLMSetDatad(dr("speed").id,0);XPLMSetDatad(dr("psi").id,0);
        XPLMSetDatai(dr("map0").id,1);XPLMSetDatai(dr("irs_align").id,0);
        float volts[2]={28,28};XPLMSetDatavf(dr("bus").id,volts,0,2);aircraft_ok=true;
        glViewport(0,0,1200,1200);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1200,0,1200,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
        {DrawingState state(graphics,restore_graphics);render_rect(0,0,1200,1200,0,true);}
        glFinish();write_bmp(std::filesystem::u8path(argv[4]));
        std::cout<<parsed.icao<<" actual plugin render_rect/draw_map: "<<route_plan.metres<<" m; range="<<view.range_nm<<" NM; route_width="<<2*route_half_width_m*transform(0).scale<<" ND pixels; GL="<<glGetString(GL_VERSION)<<'\n';
        a350_map.reset();a350_ui.reset();map_font.release();return 0;
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
