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
    if(argc!=4)return 2;
    try {
        PreviewGL gl;plugin_dir=std::filesystem::u8path(argv[1]);sim_dir=std::filesystem::u8path(argv[2]);
        auto out=std::filesystem::u8path(argv[3]);std::filesystem::create_directories(out);
        auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
        enabled=true;
        const auto project=plugin_dir;plugin_dir=out/"config-fixture";std::filesystem::create_directories(plugin_dir);
        {std::ofstream ini(plugin_dir/"config.ini");ini<<"stus_bar=1\n";}
        read_config();require(!config.scale_button,"Scale buttons must default off in old config");
        menu_action(nullptr,reinterpret_cast<void*>(42));require(config.scale_button,"Menu enables scale buttons");
        read_config();require(config.scale_button&&config.status_bar,"Enabled setting survives reload without changing status bar");
        menu_action(nullptr,reinterpret_cast<void*>(42));read_config();require(!config.scale_button,"Disabled setting survives reload");
        plugin_dir=project;
        XPLMCreateWindow_t params{};params.structSize=sizeof params;params.left=0;params.bottom=0;params.right=624;params.top=750;
        auto test_window=XPLMCreateWindowEx(&params);
        for(bool buttons:{false,true})for(bool bar:{false,true})for(auto panel:{Panel::closed,Panel::map_data,Panel::airport,Panel::status}) {
            config.scale_button=buttons;config.status_bar=bar;
            auto& state=views[2];state={};state.panel=panel;state.range_nm=1;
            auto frame=nd_controls(state,ui_model());int count=0;
            for(const auto& control:frame.controls)if(control.action==UiAction::zoom_in||control.action==UiAction::zoom_out) {
                ++count;const auto box=control.box;require(box.r-box.l==box.t-box.b,"Scale buttons must be square");
                require(bar?(box.r<=600&&box.l>500&&box.t<=stus_bar_height):(box.l<96&&box.b>=110),"Scale button placement");
                Vec2 at{(box.l+box.r)*.5,(box.b+box.t)*.5};require(frame.hit(at)->action==control.action,"Scale buttons own their hit targets");
                pointer(2,at,at,xplm_MouseDown);pointer(2,at,at,xplm_MouseUp);
                require(state.range_nm==(control.action==UiAction::zoom_in?.5:2),"Actual mouse click changes range, including status row");state.range_nm=1;
            }
            require(count==(buttons?2:0),"Scale controls only visible when enabled");
            popup_wheel(test_window,312,362,0,1,nullptr);require(state.range_nm==(buttons?1:.5),"Wheel zoom is exclusive with buttons");
            state.range_nm=1;ui_action(2,UiAction::zoom_in);require(state.range_nm==(buttons?.5:1),"Button actions are exclusive with wheel mode");
            state.panel=Panel::airport;state.results.resize(6);state.airport_first=0;state.airport_dropdown=true;
            popup_wheel(test_window,212,bar?124:92,0,-1,nullptr);require(state.airport_first>0,"Dropdown wheel scrolling remains available in either mode");
        }
        XPLMDestroyWindow(test_window);config={};
        std::cout<<"PASS saved setting, default, all panel/status layouts, square hit targets, actual clicks, wheel exclusivity and dropdown scrolling\n";
        std::vector<IndexEntry> index;
        for(const auto& f:scenery_files(sim_dir)){auto rows=scan_index(f);index.insert(index.end(),rows.begin(),rows.end());}
        auto entry=find_airport(canonical_index(index),"ZUUU");if(!entry)throw std::runtime_error("ZUUU absent");
        airport=std::make_shared<Airport>(load_airport(*entry));
        a350_map=std::make_shared<A350Map>(plugin_dir/"assets",*entry);
        a350_ui=std::make_unique<A350Map>(plugin_dir/"assets");
        if(!map_font.load(plugin_dir/"assets/font_b612_px2.bin"))throw std::runtime_error("Font absent");
        bind_refs();airport_map_loading=false;enabled=true;aircraft_ok=true;nd_on[0]=true;
        const auto& runway=airport->runways[0];
        const auto own=(runway.start+runway.end)*.5;
        const double heading=std::atan2(runway.axis().x,runway.axis().y)*180/pi;
        auto& view=views[0];view.follow=true;view.center=own;
        view.orientation=-1;view.range_nm=1;manual_airport=true;
        XPLMSetDatad(dr("lat").id,airport->lat+own.y/earth*180/pi);XPLMSetDatad(dr("lon").id,airport->lon+own.x/(earth*std::cos(airport->lat*pi/180))*180/pi);
        XPLMSetDatad(dr("psi").id,heading);XPLMSetDatad(dr("magpsi").id,heading);XPLMSetDatad(dr("speed").id,0);
        XPLMSetDatai(dr("map0").id,1);XPLMSetDatai(dr("irs_align").id,0);
        float volts[2]={28,28};XPLMSetDatavf(dr("bus").id,volts,0,2);
        glViewport(0,0,1200,1200);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1200,0,1200,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
        current_surface=surface_name(*airport,own,heading);
        if(current_surface.empty())throw std::runtime_error("Empty current surface");
        config.scale_button=true;
        for(int mode:{3})for(bool status:{false,true}) {
            XPLMSetDatai(dr("mode0").id,mode);config.status_bar=status;
            view.panel=Panel::closed;const auto closed=transform(0);
            for(auto panel:{Panel::closed,Panel::map_data,Panel::airport,Panel::status}) {
                view.panel=panel;const auto now=transform(0);
                if(length(now.anchor-closed.anchor)>1e-9||length(now.center-closed.center)>1e-9||now.scale!=closed.scale||now.radius!=closed.radius||now.angle!=closed.angle)
                    throw std::runtime_error("Actual plugin projection moves on menu toggle");
                if(panel!=Panel::closed&&panel!=Panel::map_data)continue;
                glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);
                {DrawingState state(graphics,restore_graphics);render_rect(0,0,1200,1200,0,true);}
                glFinish();if(glGetError()!=GL_NO_ERROR)throw std::runtime_error("GL error");
                write_bmp(out/(std::string("position-")+(status?"yes":"no")+"_menu-"+(panel==Panel::closed?"no.bmp":"yes.bmp")));
            }
            std::cout<<"PASS actual plugin mode="<<mode<<" status="<<status<<": all menu projections fixed; scale="<<closed.scale<<" anchor="<<closed.anchor.y<<'\n';
        }
        a350_map.reset();a350_ui.reset();map_font.release();
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}