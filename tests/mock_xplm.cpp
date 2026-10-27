// Test-only XPLM stand-in. Never install this DLL into X-Plane.
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMMenus.h>
#include <XPLMPlanes.h>
#include <XPLMUtilities.h>
#include <algorithm>
#include <cstring>
#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <vector>

struct Data {std::string name;std::vector<double> values{0};int type=xplmType_Double;std::string bytes;};
struct Win {XPLMCreateWindow_t params;};
struct Command {std::string name;XPLMCommandCallback_f handler=nullptr;void* data=nullptr;};
std::map<std::string,Data> data;
std::map<std::string,Command> commands;
std::vector<Win*> windows;
Win* focused=nullptr;
std::vector<std::pair<std::string,void*>> menu_items;
XPLMMenuHandler_f menu_handler=nullptr;
std::vector<XPLMMenuID> item_menus;
std::vector<int> item_local_indices;
std::vector<XPLMMenuCheck> item_checks;
std::map<XPLMMenuID,int> menu_sizes;
std::intptr_t next_menu=2;
XPLMFlightLoop_f loop=nullptr;XPLMDrawCallback_f draw=nullptr;
std::string plugin_path,system_path,model_path;
int setter_count=0,command_handlers=0;float elapsed=0;
bool fms_enabled=false;
std::string fms_page="IDENT",fms_origin="ZUCK02L",fms_destination="ZUUU02L",fms_alternate="ZALT";
void fms_screen(const std::string& page) {
    fms_page=page;
    for(auto& d:data)if(d.first.rfind("AirbusFBW/MCDU2",0)==0){d.second.type=xplmType_Data;d.second.bytes.clear();}
    auto put=[](const std::string& field,const std::string& value) {
        auto name="AirbusFBW/MCDU2"+field;auto& d=data[name];d.name=name;d.type=xplmType_Data;d.bytes=value;
    };
    if(page=="IDENT"){put("titlew","A320-251N");put("label2w","ACTIVE NAV DATA BASE");put("label3w","SECOND NAV DATA BASE");}
    if(page=="DIR")put("titlew","DIR TO");
    if(page=="INIT"){put("titlew","INIT");put("label1w","CO RTE");}
    if(page=="PERF")put("titlew","TAKE OFF");
    if(page=="PROG")put("titlew","A320 PROG");
    if(page=="ATC")put("titlew","ATSU DATALINK");
    if(page=="RADNAV")put("titlew","RADIO NAV");
    if(page=="MENU")put("titlew","MCDU MENU");
    if(page=="SEC")put("titlew","SEC INDEX");
    if(page=="SUBPAGE")put("titlew","UNRECOGNIZED SUBPAGE");
    if(page=="DATA"){put("titlew","DATA INDEX");put("stitlew","1/2");put("cont4w","<A/C STATUS");}
    if(page=="FUEL") {
        put("titlew","FUEL PRED");put("cont1g",fms_destination.substr(0,4)+" 0000 0.0");
        put("cont2g",fms_alternate+" 0000 0.0");
    }
    if(page=="FPLN"){
        put("stitlew","FROM         CQH5482");put("cont1g",fms_origin);put("label6w","DEST TIME DIST EFOB");put("cont6w",fms_destination+" 0038 213 11.4");
    }
}
void fms_command(const std::string& name) {
    if(!fms_enabled)return;
    if(name=="AirbusFBW/MCDU2DirTo")fms_screen("DIR");
    if(name=="AirbusFBW/MCDU2Fpln")fms_screen("FPLN");
    if(name=="AirbusFBW/MCDU2Data")fms_screen("DATA");
    if(name=="AirbusFBW/MCDU2FuelPred")fms_screen("FUEL");
    if(name=="AirbusFBW/MCDU2Init")fms_screen("INIT");
    if(name=="AirbusFBW/MCDU2Perf")fms_screen("PERF");
    if(name=="AirbusFBW/MCDU2Prog")fms_screen("PROG");
    if(name=="AirbusFBW/MCDU2ATC")fms_screen("ATC");
    if(name=="AirbusFBW/MCDU2RadNav")fms_screen("RADNAV");
    if(name=="AirbusFBW/MCDU2Menu")fms_screen("MENU");
    if(name=="AirbusFBW/MCDU2SecFpln")fms_screen("SEC");
    if(name=="AirbusFBW/MCDU2Airport")fms_screen("FPLN");
    if(name=="AirbusFBW/MCDU2LSK4L"&&fms_page=="DATA")fms_screen("IDENT");
}
extern "C" {
__declspec(dllexport) void MockConfigure(const char* plugin,const char* root,const char* model) {plugin_path=plugin;system_path=root;model_path=model;}
__declspec(dllexport) void MockSet(const char* name,int i,double v) {
    // Fixtures can configure cold aircraft providers before the plugin binds them.
    XPLMFindDataRef(name);
    auto& d=data.at(name);if(i>=static_cast<int>(d.values.size()))d.values.resize(i+1);d.values[i]=v;
}
__declspec(dllexport) double MockGet(const char* name,int i) {return data.at(name).values.at(i);}
__declspec(dllexport) int MockSetters() {return setter_count;}
__declspec(dllexport) void MockAdvance(float dt) {elapsed+=dt;if(loop)loop(dt,dt,1,nullptr);}
__declspec(dllexport) void MockClick(int x,int y) {if(!windows.empty()) {windows[0]->params.handleMouseClickFunc(windows[0],x,y,xplm_MouseDown,nullptr);windows[0]->params.handleMouseClickFunc(windows[0],x,y,xplm_MouseUp,nullptr);}}
__declspec(dllexport) int MockPointer(int win,int action,int x,int y,int right) {
    auto* w=windows.at(win);auto cb=right?w->params.handleRightClickFunc:w->params.handleMouseClickFunc;
    return cb(w,x,y,static_cast<XPLMMouseStatus>(action),nullptr);
}
__declspec(dllexport) void MockProject() {if(windows.size()>1)windows[1]->params.drawWindowFunc(windows[1],nullptr);}
__declspec(dllexport) int MockWheel(int win,int x,int y,int clicks) {auto* w=windows.at(win);return w->params.handleMouseWheelFunc(w,x,y,0,clicks,nullptr);}
__declspec(dllexport) int MockCursor(int win,int x,int y) {auto* w=windows.at(win);return w->params.handleCursorFunc(w,x,y,nullptr);}
__declspec(dllexport) void MockKey(int key) {if(focused)focused->params.handleKeyFunc(focused,static_cast<char>(key),xplm_DownFlag,0,nullptr,0);}
__declspec(dllexport) int MockFocus() {auto i=std::find(windows.begin(),windows.end(),focused);return i==windows.end()?-1:static_cast<int>(i-windows.begin());}
__declspec(dllexport) void MockCommand(const char* name) {auto& c=commands.at(name);if(c.handler)c.handler(&c,xplm_CommandBegin,c.data);fms_command(name);}
__declspec(dllexport) void MockInitFms() {fms_enabled=true;fms_screen("IDENT");}
__declspec(dllexport) void MockFmsPlan(const char* origin,const char* dest) {fms_origin=origin;fms_destination=dest;fms_screen(fms_page);}
__declspec(dllexport) void MockFmsAlternate(const char* alternate) {fms_alternate=alternate;fms_screen(fms_page);}
__declspec(dllexport) const char* MockFmsPage() {return fms_page.c_str();}
__declspec(dllexport) void MockFmsSubpage() {fms_screen("SUBPAGE");}
__declspec(dllexport) int MockResources() {return static_cast<int>(windows.size())+command_handlers+(loop?1:0)+(draw?1:0);}
__declspec(dllexport) int MockMenuItems() {return static_cast<int>(menu_items.size());}
__declspec(dllexport) const char* MockMenuLabel(int i) {return menu_items.at(i).first.c_str();}
__declspec(dllexport) int MockMenuChecked(int i) {return item_checks.at(i)==xplm_Menu_Checked;}
__declspec(dllexport) void MockMenuClick(int i) {if(menu_handler)menu_handler(nullptr,menu_items.at(i).second);}
__declspec(dllexport) void MockWindowRect(int i,int* l,int* t,int* r,int* b) {auto& w=windows.at(i)->params;*l=w.left;*t=w.top;*r=w.right;*b=w.bottom;}
__declspec(dllexport) void MockHideWindow(int i) {windows.at(i)->params.visible=0;}

XPLMDataRef XPLMFindDataRef(const char* name) {
    auto it=data.find(name);if(it!=data.end())return &it->second;
    Data d;d.name=name;std::string n=name;
    if(n.find("on_ground")!=std::string::npos) {d.values.resize(3);d.type=xplmType_IntArray;}
    if(n.find("throttle_ratio")!=std::string::npos) {d.values.resize(2);d.type=xplmType_FloatArray;}
    if(n.find("speedbrake1")!=std::string::npos) {d.values.resize(8);d.type=xplmType_FloatArray;}
    if(n.find("DUBrightness")!=std::string::npos) {d.values.assign(8,1);d.type=xplmType_FloatArray;}
    if(n.find("bus_volts")!=std::string::npos) {d.values.assign(2,28);d.type=xplmType_FloatArray;}
    if(n.find("matrix")!=std::string::npos) {d.values.resize(16);d.type=xplmType_FloatArray;}
    if(n=="sim/cockpit2/tcas/targets/modeS_id") {d.values.assign(64,0);d.type=xplmType_IntArray;}
    if(n=="sim/cockpit2/tcas/targets/position/lat"||n=="sim/cockpit2/tcas/targets/position/lon"||
       n=="sim/cockpit2/tcas/targets/position/psi"||n=="sim/cockpit2/tcas/targets/position/V_msc") {
        d.values.assign(64,0);d.type=xplmType_FloatArray;
    }
    if(n.find("MAPAvail")!=std::string::npos)d.values[0]=1;
    if(n.find("NDmode")!=std::string::npos)d.values[0]=2;
    if(n.find("acf_ICAO")!=std::string::npos)d.type=xplmType_Data;
    if(fms_enabled&&n.rfind("AirbusFBW/MCDU2",0)==0)d.type=xplmType_Data;
    if(n=="AirbusFBW/ECAMFlightPhase")d.type=xplmType_Int;
    if(n=="sim/flightmodel/position/latitude")d.values[0]=30;
    if(n=="sim/flightmodel/position/longitude")d.values[0]=120.012;
    if(n=="sim/flightmodel/position/psi")d.values[0]=90;
    if(n=="sim/flightmodel/position/groundspeed")d.values[0]=50;
    return &data.emplace(n,std::move(d)).first->second;
}
int XPLMCanWriteDataRef(XPLMDataRef) {return 1;}
XPLMDataTypeID XPLMGetDataRefTypes(XPLMDataRef r) {return static_cast<Data*>(r)->type;}
int XPLMGetDatai(XPLMDataRef r) {return static_cast<int>(static_cast<Data*>(r)->values[0]);}
float XPLMGetDataf(XPLMDataRef r) {return static_cast<float>(static_cast<Data*>(r)->values[0]);}
double XPLMGetDatad(XPLMDataRef r) {return static_cast<Data*>(r)->values[0];}
void XPLMSetDatai(XPLMDataRef r,int v) {++setter_count;static_cast<Data*>(r)->values[0]=v;}
void XPLMSetDataf(XPLMDataRef r,float v) {++setter_count;static_cast<Data*>(r)->values[0]=v;}
void XPLMSetDatad(XPLMDataRef r,double v) {++setter_count;static_cast<Data*>(r)->values[0]=v;}
int XPLMGetDatavi(XPLMDataRef r,int* out,int off,int count) {auto& v=static_cast<Data*>(r)->values;if(!out)return static_cast<int>(v.size());int n=std::max(0,std::min(count,static_cast<int>(v.size())-off));for(int i=0;i<n;++i)out[i]=static_cast<int>(v[i+off]);return n;}
int XPLMGetDatavf(XPLMDataRef r,float* out,int off,int count) {auto& v=static_cast<Data*>(r)->values;if(!out)return static_cast<int>(v.size());int n=std::max(0,std::min(count,static_cast<int>(v.size())-off));for(int i=0;i<n;++i)out[i]=static_cast<float>(v[i+off]);return n;}
void XPLMSetDatavi(XPLMDataRef r,int* in,int off,int count) {++setter_count;auto& v=static_cast<Data*>(r)->values;for(int i=0;i<count;++i)v.at(i+off)=in[i];}
void XPLMSetDatavf(XPLMDataRef r,float* in,int off,int count) {++setter_count;auto& v=static_cast<Data*>(r)->values;for(int i=0;i<count;++i)v.at(i+off)=in[i];}
int XPLMGetDatab(XPLMDataRef r,void* out,int off,int count) {auto& d=*static_cast<Data*>(r);const auto str=d.name=="sim/aircraft/view/acf_ICAO"?std::string("A20N"):d.bytes;
    if(!out)return static_cast<int>(str.size()+1);int n=std::max(0,std::min(count,static_cast<int>(str.size()+1)-off));std::memcpy(out,str.c_str()+off,n);return n;}
int XPLMRegisterDrawCallback(XPLMDrawCallback_f cb,XPLMDrawingPhase,int,void*) {draw=cb;return 1;}
int XPLMUnregisterDrawCallback(XPLMDrawCallback_f,XPLMDrawingPhase,int,void*) {draw=nullptr;return 1;}
XPLMWindowID XPLMCreateWindowEx(XPLMCreateWindow_t* p) {auto* w=new Win{*p};windows.push_back(w);return w;}
void XPLMDestroyWindow(XPLMWindowID id) {auto* w=static_cast<Win*>(id);if(focused==w)focused=nullptr;windows.erase(std::remove(windows.begin(),windows.end(),w),windows.end());delete w;}
void XPLMGetScreenBoundsGlobal(int* l,int* t,int* r,int* b) {if(l)*l=0;if(t)*t=1080;if(r)*r=1920;if(b)*b=0;}
void XPLMGetWindowGeometry(XPLMWindowID id,int* l,int* t,int* r,int* b) {const auto& w=static_cast<Win*>(id)->params;*l=w.left;*t=w.top;*r=w.right;*b=w.bottom;}
void XPLMSetWindowGeometry(XPLMWindowID id,int l,int t,int r,int b) {auto& w=static_cast<Win*>(id)->params;w.left=l;w.top=t;w.right=r;w.bottom=b;}
int XPLMGetWindowIsVisible(XPLMWindowID id) {return static_cast<Win*>(id)->params.visible;}
void XPLMSetWindowIsVisible(XPLMWindowID id,int v) {static_cast<Win*>(id)->params.visible=v;}
void XPLMSetWindowResizingLimits(XPLMWindowID,int,int,int,int) {}
void XPLMSetWindowTitle(XPLMWindowID,const char*) {}
void XPLMTakeKeyboardFocus(XPLMWindowID id) {auto* next=static_cast<Win*>(id);if(focused&&focused!=next)focused->params.handleKeyFunc(focused,0,0,0,nullptr,1);focused=next;}
void XPLMSetGraphicsState(int,int,int,int,int,int,int) {}
void XPLMGenerateTextureNumbers(int* out,int count) {for(int i=0;i<count;++i)out[i]=i+1;}
void XPLMBindTexture2d(int,int) {}
void XPLMDrawString(float*,int,int,const char*,int*,XPLMFontID) {}
XPLMPluginID XPLMGetMyID() {return 42;}
void XPLMGetVersions(int* xp,int* sdk,XPLMHostApplicationID* host) {*xp=120000;*sdk=400;*host=xplm_Host_XPlane;}
void XPLMGetPluginInfo(XPLMPluginID,char*,char* path,char*,char*) {if(path)std::strcpy(path,plugin_path.c_str());}
void XPLMEnableFeature(const char*,int) {}
XPLMMenuID XPLMFindPluginsMenu() {return reinterpret_cast<void*>(1);}
XPLMMenuID XPLMCreateMenu(const char*,XPLMMenuID,int,XPLMMenuHandler_f handler,void*) {menu_handler=handler;return reinterpret_cast<void*>(next_menu++);}
void XPLMDestroyMenu(XPLMMenuID menu) {menu_sizes.erase(menu);
    if(menu_sizes.empty()) {menu_handler=nullptr;menu_items.clear();item_menus.clear();item_local_indices.clear();item_checks.clear();next_menu=2;}}
int XPLMAppendMenuItem(XPLMMenuID menu,const char* label,void* item,int) {if(menu==reinterpret_cast<void*>(1))return 0;
    const int index=menu_sizes[menu]++;
    menu_items.emplace_back(label,item);item_menus.push_back(menu);item_local_indices.push_back(index);item_checks.push_back(xplm_Menu_NoCheck);return index;}
void XPLMRemoveMenuItem(XPLMMenuID,int) {}
void XPLMCheckMenuItem(XPLMMenuID menu,int index,XPLMMenuCheck check) {
    for(size_t i=0;i<menu_items.size();++i)if(item_menus[i]==menu&&item_local_indices[i]==index)item_checks[i]=check;
}
void XPLMGetNthAircraftModel(int,char* name,char* path) {std::strcpy(name,"a320.acf");std::strcpy(path,model_path.c_str());}
float XPLMGetElapsedTime() {return elapsed;}
int XPLMGetCycleNumber() {return static_cast<int>(elapsed*60);}
void XPLMRegisterFlightLoopCallback(XPLMFlightLoop_f cb,float,void*) {loop=cb;}
void XPLMUnregisterFlightLoopCallback(XPLMFlightLoop_f,void*) {loop=nullptr;}
void XPLMGetSystemPath(char* path) {std::strcpy(path,system_path.c_str());}
void XPLMDebugString(const char*) {}
XPLMCommandRef XPLMFindCommand(const char* name) {auto& c=commands[name];c.name=name;return &c;}
XPLMCommandRef XPLMCreateCommand(const char* name,const char*) {return XPLMFindCommand(name);}
void XPLMCommandOnce(XPLMCommandRef id) {auto& c=*static_cast<Command*>(id);if(c.handler)c.handler(id,xplm_CommandBegin,c.data);fms_command(c.name);if(c.handler)c.handler(id,xplm_CommandEnd,c.data);}
void XPLMRegisterCommandHandler(XPLMCommandRef id,XPLMCommandCallback_f cb,int,void* ptr) {auto& c=*static_cast<Command*>(id);c.handler=cb;c.data=ptr;++command_handlers;}
void XPLMUnregisterCommandHandler(XPLMCommandRef id,XPLMCommandCallback_f,int,void*) {static_cast<Command*>(id)->handler=nullptr;--command_handlers;}
}
