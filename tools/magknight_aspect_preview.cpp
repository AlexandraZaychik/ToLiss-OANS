// Offline WGL preview of the ACTUAL plugin draw_map/render_rect functions.
// The simulator API is supplied by the GL-enabled test SDK; no simulator UI.
#include "../src/plugin.cpp"
#include <iostream>
#include "../src/aircraft/magknight/backend.hpp"
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
 if(argc!=3)return 2;
 try {
  PreviewGL gl;plugin_dir=std::filesystem::u8path(argv[1]);
  auto out=std::filesystem::u8path(argv[2]);std::filesystem::create_directories(out);
  auto require=[](bool ok,const char* message){if(!ok)throw std::runtime_error(message);};
  enabled=aircraft_ok=profile_ok=true;aircraft_backend=&magknight::backend();
  for(const auto& binding:simulator_names)refs[binding.first].bind(binding.second.c_str());
  for(const auto& binding:aircraft_backend->bindings)refs[binding.first].bind(binding.second.c_str());
  auto set=[](const char* key,double v){XPLMSetDatad(dr(key).id,v);};
  set("mk_nav",1);set("mk_irs",1);set("mk_brightness",10);set("range0",5);
  std::string error;require(profile::load(plugin_dir/"config/magknight","B788",error),error.c_str());
  auto fixture=out/"aspect-airport.dat";
  {std::ofstream f(fixture);f<<"1 0 0 0 ZTST Aspect airport\n100 45 1 0 0.25 0 0 0 09 30.0 120.0 0 0 0 0 0 0 27 30.0 120.035 0 0 0 0 0 0\n99\n";}
  IndexEntry entry;entry.icao="ZTST";entry.file=fixture;
  airport=std::make_shared<Airport>(load_airport(entry));
  a350_map=std::make_shared<A350Map>(plugin_dir/"assets",entry);
  a350_ui=std::make_unique<A350Map>(plugin_dir/"assets");
  require(map_font.load(plugin_dir/"assets/font_b612_px2.bin"),"Font load");
  const auto bounds=a350_ui->oans_switch(false).bounds;
  oans_base_box={bounds[0],bounds[1],bounds[2],bounds[3]};set_oans_vertical_offset(0);
  set("lat",30);set("lon",120.0175);set("psi",0);set("magpsi",0);
  airport_map_loading=false;first_oans_open=false;manual_airport=true;views[0].range_nm=1;
  glViewport(0,0,1200,1200);glMatrixMode(GL_PROJECTION);glLoadIdentity();glOrtho(0,1200,0,1200,-1,1);glMatrixMode(GL_MODELVIEW);glLoadIdentity();
  for(int mode:{4,2})for(int state:{0,1,2}) {
   set("mode0",mode);views[0].range_nm=mode==4?100:1;nd_on[0]=state;const double aspect=nd_aspect(0),height=800,width=height*aspect,left=(1200-width)/2,bottom=200;
   glClearColor(.12,.12,.12,1);glClear(GL_COLOR_BUFFER_BIT);
   const float native[]={.08f,.15f,.22f};rect(left,bottom,left+width,bottom+height,native);
   std::vector<unsigned char> before(1200*1200*3);glPixelStorei(GL_PACK_ALIGNMENT,1);
   glReadPixels(0,0,1200,1200,GL_RGB,GL_UNSIGNED_BYTE,before.data());
   if(state)clear_nd_rect(left,bottom,width,height);
   {DrawingState drawing_state(graphics,restore_graphics);render_rect(left,bottom,width,height,0,true);}
   glFinish();require(glGetError()==GL_NO_ERROR,"OpenGL error");
   write_bmp(out/((mode==4?std::string("plan-"):std::string("arc-"))+(state==0?"off.bmp":state==1?"half.bmp":"full.bmp")));
   {
    std::vector<unsigned char> after(1200*1200*3);glReadPixels(0,0,1200,1200,GL_RGB,GL_UNSIGNED_BYTE,after.data());
    int checked=0;
    for(int y=int(std::ceil(bottom+height*552/600))+1;y<int(bottom+height)-1;y+=3)
     for(int x=int(std::ceil(left))+1;x<int(left+width)-1;x+=3) {
      const auto i=(y*1200+x)*3;
      const bool same=before[i]==after[i]&&before[i+1]==after[i+1]&&before[i+2]==after[i+2];
      require(same==(state==0),"Full OANS header replacement/off restoration failed");++checked;
     }
    require(checked>1000,"Header sample too small");
    require(std::abs(oans_hit_box(0).b-490)<.01,"MK787 button must match old Test Y=+40");
    click_inverse[0]=Homography{{1./600,0,0,0,-1./600,1,0,0,1}};
    Vec2 mapped;
    require(nd_point(0,100,570,mapped),"Full OANS upper input unavailable");
    require(nd_point(0,100,400,mapped),"Map input unavailable");
    std::cout<<"PASS full OANS header and Y=+40 mode="<<mode<<" state="<<state<<'\n';
   }
   if(mode==4&&state!=0) {
    std::vector<unsigned char> pixels(1200*1200*3);glPixelStorei(GL_PACK_ALIGNMENT,1);glReadPixels(0,0,1200,1200,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    const double cx=left+width*.5,cy=bottom+transform(0).anchor.y*height/600;
    int l=1200,r=-1,b=1200,t=-1,count=0;
    for(int y=200;y<940;++y)for(int x=int(left);x<int(left+width);++x) {
     auto p=&pixels[(y*1200+x)*3];if(p[0]==230&&p[1]==230&&p[2]==230) {if(std::abs(y-cy)<3){l=std::min(l,x);r=std::max(r,x);}
      if(std::abs(x-cx)<3){b=std::min(b,y);t=std::max(t,y);}
      ++count;}
    }
    require(count>500,"Compass ring pixel sample missing");
    const double ratio=double(r-l+1)/(t-b+1);
    std::cout<<"PLAN state="<<state<<" physical_aspect="<<aspect<<" ring_pixels="<<r-l+1<<'x'<<t-b+1<<" ratio="<<ratio<<'\n';
    require(std::abs(ratio-1)<.02,"Actual compass ring is stretched");
   }
   const auto hit=oans_hit_box(0);const Vec2 at{(hit.l+hit.r)/2,(hit.b+hit.t)/2};
   pointer(0,at,at,xplm_MouseDown);pointer(0,at,at,xplm_MouseUp);
   require(nd_on[0]==(state+1)%3,"Aspect-correct switch click misses its visible bounds");
   require(render_aspect==1,"Display aspect leaked to another view");
  }
  a350_map.reset();a350_ui.reset();map_font.release();
  std::cout<<"PASS restored full OANS, standard Y=+40, circular compass and switch hit bounds\n";
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
