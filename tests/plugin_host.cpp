// Standalone native loader test. This executable does not start or attach to X-Plane.
#include <windows.h>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <sstream>
#include <utility>
#include <cstdlib>
#include "../src/aircraft_profile.hpp"
#include "../src/aircraft_profile_generator.hpp"

template<class T>T function(HMODULE m,const char* n) {auto p=GetProcAddress(m,n);if(!p)throw std::runtime_error(std::string("Missing export ")+n);return reinterpret_cast<T>(p);}
int checks=0;
void check(bool v,const char* msg) {++checks;if(!v)throw std::runtime_error(msg);}
int main(int argc,char** argv) {
    // This process is a command-line test tool: report errors on stderr, not as
    // modal Windows crash/reporting dialogs on the user's desktop.
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    // Some MinGW MSVCRT import libraries do not expose newer CRT controls.
    // Apply them only when the process's existing CRT actually exports them.
    if(auto crt=GetModuleHandleW(L"msvcrt.dll")) {
        auto abort_mode=reinterpret_cast<unsigned(*)(unsigned,unsigned)>(GetProcAddress(crt,"_set_abort_behavior"));
        if(abort_mode)abort_mode(0,_WRITE_ABORT_MSG|_CALL_REPORTFAULT);
        auto error_mode=reinterpret_cast<int(*)(int)>(GetProcAddress(crt,"_set_error_mode"));
        if(error_mode)error_mode(2); // _OUT_TO_STDERR
    }
    const bool failure_test=argc==6&&std::string(argv[5])=="--test-failure-cleanup";
    const bool cache_test=argc==6&&std::string(argv[5])=="--test-dsf-cache";
    const bool reextract_test=argc==6&&std::string(argv[5])=="--test-dsf-reextract";
    const bool osm_cache_test=argc==6&&std::string(argv[5])=="--test-osm-cache";
    const bool building_only=cache_test||reextract_test||osm_cache_test;
    if(argc!=5&&!failure_test&&!building_only) {std::cerr<<"Usage: plugin_host MOCK_DLL TEST_XPL SIM_FIXTURE MODEL_PATH [--test-failure-cleanup|--test-dsf-cache|--test-dsf-reextract|--test-osm-cache]\n";return 2;}
    HMODULE mock=nullptr,plugin=nullptr;void(*shutdown)()=nullptr;
    try {
        mock=LoadLibraryExW(std::filesystem::u8path(argv[1]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        check(mock!=nullptr,"load isolated mock SDK");
        function<void(*)(const char*,const char*,const char*)>(mock,"MockConfigure")(argv[2],argv[3],argv[4]);
        auto set=function<void(*)(const char*,int,double)>(mock,"MockSet");auto get=function<double(*)(const char*,int)>(mock,"MockGet");
        auto advance=function<void(*)(float)>(mock,"MockAdvance");
        auto pointer=function<int(*)(int,int,int,int,int)>(mock,"MockPointer");
        auto project=function<void(*)()>(mock,"MockProject");auto key=function<void(*)(int)>(mock,"MockKey");
        auto focus=function<int(*)()>(mock,"MockFocus");auto wheel=function<int(*)(int,int,int,int)>(mock,"MockWheel");
        auto cursor=function<int(*)(int,int,int)>(mock,"MockCursor");
        auto cmd=function<void(*)(const char*)>(mock,"MockCommand");auto resources=function<int(*)()>(mock,"MockResources");
        auto writes=function<int(*)()>(mock,"MockSetters");
        const auto package=std::filesystem::u8path(argv[2]).parent_path().parent_path();
        std::string profile_error;
        check(profile::load(package/"config","A20N",profile_error),profile_error.c_str());
        const auto expected_profile=profile::current;
        const auto base_profile=package/"config/a20n.json";
        std::ifstream base_file(base_profile,std::ios::binary);
        const std::string base_text((std::istreambuf_iterator<char>(base_file)),std::istreambuf_iterator<char>());
        base_file.close();
        std::string profile_id;
        check(profile::identify(std::filesystem::u8path(argv[4]),profile_id,profile_error),profile_error.c_str());
        const auto id_profile=package/"config"/(profile_id+".json");
        if(!failure_test) {std::filesystem::remove(base_profile);std::filesystem::remove(id_profile);}
        const auto log_path=package/"diagnostics.log";
        plugin=LoadLibraryExW(std::filesystem::u8path(argv[2]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        check(plugin!=nullptr,"Windows loads actual .xpl binary and its imports");
        auto start=function<int(*)(char*,char*,char*)>(plugin,"XPluginStart");auto enable=function<int(*)()>(plugin,"XPluginEnable");
        auto disable=function<void(*)()>(plugin,"XPluginDisable");auto stop=function<void(*)()>(plugin,"XPluginStop");
        auto message=function<void(*)(int,int,void*)>(plugin,"XPluginReceiveMessage");
        shutdown=stop;
        char name[256],sig[256],desc[256];check(start(name,sig,desc)==1,"native plugin start");check(enable()==1,"enable");
        if(!failure_test) {
            check(profile::load_file(id_profile,profile_error),"startup generated a loadable ID-named aircraft JSON profile");
            check(!std::filesystem::exists(base_profile),"startup never generates a base aircraft profile");
            for(int side=0;side<2;++side)for(int corner=0;corner<4;++corner)for(int axis=0;axis<3;++axis)
                check(std::abs(profile::current.nd_corners[side][corner][axis]-expected_profile.nd_corners[side][corner][axis])<1e-9,"generated click corners match reference");
            // Restore the distributed fixture for subsequent lifecycle test runs.
            std::ofstream restored(base_profile,std::ios::binary);restored<<base_text;
        }
        {
            std::ifstream detail_file(package/"detail.log");std::ostringstream detail_text;detail_text<<detail_file.rdbuf();
            const auto text=detail_text.str();
            check(text.find("CONFIG]")!=std::string::npos&&text.find("Profile ID=")!=std::string::npos,"detail.log records configuration lookup and matching rule");
            const bool debug=std::filesystem::u8path(argv[2]).u8string().find("mock-host-debug")!=std::string::npos;
            check(text.find(debug?"build=debug":"build=release")!=std::string::npos,"detail.log identifies actual build flavor");
            check((text.find("ENTER]")!=std::string::npos)==debug,"only debug logs callback call chains");
            check((text.find("DISPLAY] ND_INSTALL path=")!=std::string::npos&&
                text.find("ND_INSTALL WARNING: unexpected plugin directory name")!=std::string::npos&&
                text.find("ND_REGISTER callback_registered=1 final_visibility=UNVERIFIED")!=std::string::npos)==debug,
                "only debug detail logs expose installation risk and callback registration without claiming visibility");
            std::ifstream display_file(log_path);std::ostringstream display_text;display_text<<display_file.rdbuf();
            check((display_text.str().find("ND_INSTALL WARNING:")!=std::string::npos&&
                display_text.str().find("ND_REGISTER callback_registered=1")!=std::string::npos)==debug,
                "only debug diagnostics.log records display installation and registration evidence");
            if(debug)check(text.find("Environment X-Plane=120000")!=std::string::npos&&text.find("DataRef bind")!=std::string::npos,"debug records simulator version and SDK binding breadcrumbs");
        }
        check(resources()==24,"4 windows, 18 OANS command handlers, loop and draw callback registered");
        auto menu_count=function<int(*)()>(mock,"MockMenuItems");auto menu_label=function<const char*(*)(int)>(mock,"MockMenuLabel");
        auto menu_click=function<void(*)(int)>(mock,"MockMenuClick");
        check(menu_count()==15&&std::string(menu_label(0))=="Open Airport Map Window"&&std::string(menu_label(1))=="Test","map window, Test and Setting menu hierarchy registered");
        auto menu_checked=function<int(*)(int)>(mock,"MockMenuChecked");
        check(std::string(menu_label(2))=="Setting"&&std::string(menu_label(3))=="Stand name"&&std::string(menu_label(4))=="Simple"&&std::string(menu_label(5))=="Complex","stand name submenu labels");
        check(menu_checked(4)&&!menu_checked(5),"Simple is selected by default");
        menu_click(5);check(!menu_checked(4)&&menu_checked(5),"Complex selection is exclusive");
        {std::ifstream saved(package/"config.ini");std::ostringstream contents;contents<<saved.rdbuf();
         check(contents.str().find("stand_name=complex")!=std::string::npos,"Complex preference is persisted");}
        menu_click(4);check(menu_checked(4)&&!menu_checked(5),"Simple selection is exclusive");
        menu_click(5); // Existing browser regressions use complete original names.
        check(std::string(menu_label(6))=="stus_bar"&&!menu_checked(6),"optional surface status row defaults off");
        menu_click(6);check(menu_checked(6),"status row toggle enables checkmark");
        {std::ifstream saved(package/"config.ini");std::ostringstream contents;contents<<saved.rdbuf();
         check(contents.str().find("stus_bar=1")!=std::string::npos&&contents.str().find("stand_name=complex")!=std::string::npos,"status row persists without overwriting stand setting");}
        menu_click(6);check(!menu_checked(6),"status row can be disabled");
        check(std::string(menu_label(13))=="ADS-B IN"&&!menu_checked(13),"ADS-B IN defaults off");
        if(failure_test) {menu_click(4);throw std::runtime_error("EXPECTED_CLEANUP_REGRESSION");}
        // Cold cockpit: simulator coordinates exist, but the aircraft has no IRS position.
        set("AirbusFBW/CaptMAPAvail",0,0);set("AirbusFBW/CoMAPAvail",0,0);
        set("AirbusFBW/TimeToAlign",0,480);
        set("sim/cockpit2/electrical/bus_volts",0,0);set("sim/cockpit2/electrical/bus_volts",1,0);
        // X-Plane TCAS arrays are mocked as 64 slots. Slot zero is ownship;
        // of the remaining nearby targets only the valid sub-80-knot one counts.
        set("sim/cockpit2/tcas/targets/modeS_id",0,999);
        set("sim/cockpit2/tcas/targets/position/lat",0,30);
        set("sim/cockpit2/tcas/targets/position/lon",0,120.012);
        set("sim/cockpit2/tcas/targets/position/V_msc",0,0);
        set("sim/cockpit2/tcas/targets/modeS_id",1,1001);
        set("sim/cockpit2/tcas/targets/position/lat",1,30.0005);
        set("sim/cockpit2/tcas/targets/position/lon",1,120.0125);
        set("sim/cockpit2/tcas/targets/position/psi",1,90);
        set("sim/cockpit2/tcas/targets/position/V_msc",1,35);
        set("sim/cockpit2/tcas/targets/position/vx",1,3);
        set("sim/cockpit2/tcas/targets/position/vz",1,4);
        set("sim/cockpit2/tcas/targets/modeS_id",2,1002);
        set("sim/cockpit2/tcas/targets/position/lat",2,30.0003);
        set("sim/cockpit2/tcas/targets/position/lon",2,120.012);
        set("sim/cockpit2/tcas/targets/position/V_msc",2,45);
        set("sim/cockpit2/tcas/targets/modeS_id",3,0);
        set("sim/cockpit2/tcas/targets/position/lat",3,30.0003);
        set("sim/cockpit2/tcas/targets/position/lon",3,120.012);
        set("sim/cockpit2/tcas/targets/position/V_msc",3,10);
        set("sim/cockpit2/tcas/targets/modeS_id",4,1004);
        set("sim/cockpit2/tcas/targets/position/lat",4,30.02);
        set("sim/cockpit2/tcas/targets/position/lon",4,120.012);
        set("sim/cockpit2/tcas/targets/position/V_msc",4,10);
        // Allow only the plugin's file worker to load the tiny test scenery.
        for(int i=0;i<250;++i) {advance(.1f);Sleep(5);}
        auto read=[](const std::filesystem::path& p) {std::ifstream in(p);std::ostringstream s;s<<in.rdbuf();return s.str();};
        auto session_log=[&]() {std::ifstream in(log_path,std::ios::binary);std::ostringstream s;s<<in.rdbuf();return s.str();};
        auto report=[&]() {cmd("toliss_oans/diagnostics");return read(package/"interfaces.txt");};
        check((session_log().find("ND_DRAW health=NO_GAUGE_CALLBACK_YET")!=std::string::npos&&
            read(package/"detail.log").find("DISPLAY] ND_DISPLAY side=0")!=std::string::npos)==
            (std::filesystem::u8path(argv[2]).u8string().find("mock-host-debug")!=std::string::npos),
            "only debug logs missing draw callbacks and ND blocking state without a manual report");
        check(report().find("OANS_EMPTY_MAP message=PLEASE WAIT\n")!=std::string::npos,
              "waiting for position or map data displays PLEASE WAIT");
        check(report().find("OANS_POSITION ready=0 pending=1 first_open=1 manual=0 airport=NONE requested=\n")!=std::string::npos,
              "cold cockpit cannot choose an airport from simulator coordinates before IRS position is available");
        set("sim/cockpit2/electrical/bus_volts",0,28);set("sim/cockpit2/electrical/bus_volts",1,28);
        for(int i=0;i<60;++i) {advance(.1f);Sleep(2);}
        check(report().find("OANS_POSITION ready=0 pending=1 first_open=1 manual=0 airport=NONE requested=\n")!=std::string::npos,
              "electrical power alone does not confirm the first airport while MAP NOT AVAIL");
        check(report().find("TCAS traffic_icons=0 ")!=std::string::npos,"ADS-B IN off suppresses targets");
        menu_click(13);advance(.2f);
        check(menu_checked(13)&&read(package/"config.ini").find("adsb_in=1")!=std::string::npos,"ADS-B IN enabled and persisted");
        cmd("toliss_oans/reload");advance(.2f);
        check(menu_checked(13)&&report().find("ADS_B_TARGET slot=1 flight=AFR2545 gs_kt=10 type=B77W")!=std::string::npos,
              "ADS-B preference reloads and eight-byte metadata plus horizontal GS are read during alignment");
        menu_click(13);
        check(!menu_checked(13)&&report().find("TCAS traffic_icons=0 ")!=std::string::npos,"ADS-B IN off immediately clears targets");
        menu_click(13);advance(.2f);
        set("AirbusFBW/CaptMAPAvail",0,1);set("AirbusFBW/CoMAPAvail",0,1);
        for(int i=0;i<40;++i) {advance(.1f);Sleep(2);}
        check(report().find("OANS_POSITION ready=0 pending=1 first_open=1 manual=0 airport=NONE requested=\n")!=std::string::npos&&
              report().find("TCAS traffic_icons=1 ")!=std::string::npos,
              "MAP AVAIL during IRS alignment hides ownship but ADS-B traffic remains available");
        set("AirbusFBW/TimeToAlign",0,0);
        set("sim/flightmodel/position/latitude",0,0);set("sim/flightmodel/position/longitude",0,0);
        for(int i=0;i<20;++i) {advance(.1f);Sleep(2);}
        check(report().find("OANS_EMPTY_MAP message=ARPT NAV NOT AVAIL\n")!=std::string::npos,
              "completed nearest-airport search without an airport displays ARPT NAV NOT AVAIL");
        set("sim/flightmodel/position/latitude",0,30);set("sim/flightmodel/position/longitude",0,120.012);
        cmd("toliss_oans/nearest");
        check(report().find("OANS_EMPTY_MAP message=PLEASE WAIT\n")!=std::string::npos,
              "new airport search returns to PLEASE WAIT");
        for(int i=0;i<80;++i) {advance(.1f);Sleep(2);}
        check(report().find("OANS_POSITION ready=1 pending=0 first_open=1 manual=0 airport=ZTEST ")!=std::string::npos,
              "aircraft MAP AVAIL confirms current position and loads its nearest airport");
        const std::string expected_buildings=cache_test?"Building source: reused DSF cache":
            osm_cache_test?"Building fallback: reused cached OSM data":"Building source: extracted runtime scenery DSF:";
        for(int i=0;i<100&&session_log().find(expected_buildings)==std::string::npos;++i) {advance(.1f);Sleep(5);}
        check(session_log().find(expected_buildings)!=std::string::npos,
              "actual plugin selects the expected DSF cache/extraction/OSM source");
        if(!osm_cache_test)check(std::filesystem::exists(package/"cache/dsf_buildings_ZTEST.json"),"actual plugin retains usable DSF cache");
        if(building_only) {
            check(session_log().find("downloaded and cached OSM")==std::string::npos,"cache/extraction path needs no network download");
            disable();stop();shutdown=nullptr;check(resources()==0,"building test stops workers and releases SDK resources");
            check(FreeLibrary(plugin)!=0,"unload plugin after building test");plugin=nullptr;FreeLibrary(mock);mock=nullptr;
            std::cout<<"PASS "<<expected_buildings<<" (MOCK SDK ONLY)\n";return 0;
        }
        check(report().find("TCAS traffic_icons=1 ")!=std::string::npos,
              "TCAS array reader skips ownship, invalid slots, fast targets and targets beyond 1 NM");
        const double old_lat=get("sim/flightmodel/position/latitude",0),old_lon=get("sim/flightmodel/position/longitude",0);
        const double old_heading=get("sim/flightmodel/position/psi",0);
        double old_ground[3];for(int i=0;i<3;++i) {old_ground[i]=get("sim/flightmodel2/gear/on_ground",i);set("sim/flightmodel2/gear/on_ground",i,1);}
        menu_click(6);set("sim/flightmodel/position/latitude",0,30);set("sim/flightmodel/position/longitude",0,120.01);set("sim/flightmodel/position/psi",0,90);advance(.2f);
        check(report().find("OANS_STUS_BAR enabled=1 surface=09\n")!=std::string::npos,"actual plugin detects ownship runway for status row");
        set("sim/flightmodel/position/psi",0,270);advance(.2f);
        check(report().find("OANS_STUS_BAR enabled=1 surface=09\n")!=std::string::npos,"surface identification does not resample before 0.5 seconds");
        advance(.31f);
        check(report().find("OANS_STUS_BAR enabled=1 surface=27\n")!=std::string::npos,"actual plugin follows reciprocal runway heading");
        for(int i=0;i<3;++i)set("sim/flightmodel2/gear/on_ground",i,0);advance(.2f);
        check(report().find("OANS_STUS_BAR enabled=1 surface=\n")!=std::string::npos,"airborne status row clears previous runway");
        menu_click(6);set("sim/flightmodel/position/latitude",0,old_lat);set("sim/flightmodel/position/longitude",0,old_lon);set("sim/flightmodel/position/psi",0,old_heading);
        for(int i=0;i<3;++i)set("sim/flightmodel2/gear/on_ground",i,old_ground[i]);advance(.2f);
        set("sim/cockpit2/tcas/targets/position/V_msc",1,80*.514444444444+.1);
        advance(.2f);
        check(report().find("TCAS traffic_icons=0 ")!=std::string::npos,"TCAS traffic icon disappears at the 80 knot threshold");
        set("sim/cockpit2/tcas/targets/position/V_msc",1,35);
        auto row=[&](int id) {auto s=report();auto begin=s.find("view="+std::to_string(id)+" ");return s.substr(begin,s.find('\n',begin)-begin);};
        cmd("toliss_oans/fo_airport");
        check(report().find("ND_state side=1 enabled=0 blocked=SIDE MAP SWITCH OFF")!=std::string::npos&&focus()==-1,"FO panel command cannot bypass unchecked OANS");
        check(report().find("ND_state side=0 enabled=0 blocked=SIDE MAP SWITCH OFF")!=std::string::npos,"captain OANS also defaults unchecked");
        // A deterministic perspective camera faces the real, audited captain ND corners.
        // This exercises the .xpl's overlay projection and event callbacks without a sim.
        set("sim/flightmodel/position/psi",0,0);
        set("sim/graphics/view/view_type",0,1026);set("sim/graphics/view/window_width",0,1920);set("sim/graphics/view/window_height",0,1080);
        set("sim/graphics/view/pilots_head_x",0,-.36);set("sim/graphics/view/pilots_head_y",0,1.45);set("sim/graphics/view/pilots_head_z",0,-14);
        for(int i=0;i<16;++i) {set("sim/graphics/view/world_matrix",i,0);set("sim/graphics/view/projection_matrix_3d",i,0);}
        for(int i:{0,5,10,15})set("sim/graphics/view/world_matrix",i,1);
        set("sim/graphics/view/world_matrix",12,.36);set("sim/graphics/view/world_matrix",13,-1.45);set("sim/graphics/view/world_matrix",14,14);
        for(int i=0;i<16;++i)set("sim/graphics/view/acf_matrix",i,get("sim/graphics/view/world_matrix",i));
        set("sim/graphics/view/projection_matrix_3d",0,2);set("sim/graphics/view/projection_matrix_3d",5,2);
        set("sim/graphics/view/projection_matrix_3d",10,-1);set("sim/graphics/view/projection_matrix_3d",11,-1);set("sim/graphics/view/projection_matrix_3d",14,-.2);
        project();
        auto screen=[](double x,double y,int side=0) {
            const auto& q=profile::current.nd_corners[side];double p[3];
            for(int j=0;j<3;++j)p[j]=q[0][j]+x/600*(q[1][j]-q[0][j])+(1-y/600)*(q[3][j]-q[0][j]);
            const double z=-(p[2]+14);
            return std::make_pair(static_cast<int>(std::lround((2*(p[0]+.36)/z*.5+.5)*1920)),static_cast<int>(std::lround((2*(p[1]-1.45)/z*.5+.5)*1080)));
        };
        auto click=[&](double x,double y) {auto p=screen(x,y);pointer(1,1,p.first,p.second,0);pointer(1,3,p.first,p.second,0);};
        auto switch_state=[&](bool visible,bool checked) {return report().find("OANS_switch side=0 visible="+std::to_string(visible)+" checked="+std::to_string(checked))!=std::string::npos;};
        auto centre=screen(300,280);
        check(cursor(1,centre.first,centre.second)==0,"unchecked OANS retains the aircraft cursor");
        check(switch_state(true,false),"10 NM exposes unchecked OANS over original ND");
        check(pointer(1,1,centre.first,centre.second,0)==0&&pointer(1,1,centre.first,centre.second,1)==0&&wheel(1,centre.first,centre.second,1)==0,
              "unchecked OANS passes map clicks, right clicks and wheel through to aircraft");
        // A new position only 22 km away would have retained the previous airport
        // under the old 60 km automatic-selection threshold.
        set("sim/flightmodel/position/latitude",0,30.2);
        click(560,506);
        check(switch_state(true,true)&&report().find("ND_state side=1 enabled=0")!=std::string::npos,"projected OANS checkbox enables captain only");
        check(report().find("OANS_POSITION ready=1 pending=1 first_open=0 manual=0 airport=NONE requested=\n")!=std::string::npos,
              "first OANS activation clears the preloaded airport before checking the current position");
        for(int i=0;i<80;++i) {advance(.1f);Sleep(2);}
        check(report().find("OANS_POSITION ready=1 pending=0 first_open=0 manual=0 airport=ZNEAR ")!=std::string::npos,
              "first OANS activation chooses the current airport even within 60 km of the old one");
        set("sim/flightmodel/position/latitude",0,30);cmd("toliss_oans/nearest");
        for(int i=0;i<80;++i) {advance(.1f);Sleep(2);}
        check(cursor(1,centre.first,centre.second)==1&&row(0).find("cursor=1")!=std::string::npos,"enabled OANS replaces OS pointer with map cursor");
        menu_click(6);click(48,19);
        check(row(0).find("panel=1")!=std::string::npos,"left status row arrow opens panel through projected hit target");
        click(48,45);
        check(row(0).find("panel=3")!=std::string::npos,"status row lifts STATUS button with its hit target");
        click(180,19);
        check(row(0).find("panel=3")!=std::string::npos,"black status row cannot activate the old STATUS button");
        click(48,4);
        check(row(0).find("panel=3")!=std::string::npos,"bottom crop safety margin cannot activate arrow");
        click(48,19);menu_click(6);
        check(row(0).find("panel=0")!=std::string::npos,"same status row arrow closes the panel");
        auto projection_row=[&]() {auto s=report();auto begin=s.find("click_projection ");return s.substr(begin,s.find('\n',begin)-begin);};
        project();const auto near_projection=projection_row();
        // A stationary cockpit at a distant world origin must not move its hit
        // map when float world translations round by a few millimetres.
        for(double delta:{0.0,.001953125,-.001953125,.00390625}) {
            set("sim/flightmodel/position/local_x",0,-18456.7);
            set("sim/flightmodel/position/local_y",0,-148.21);
            set("sim/flightmodel/position/local_z",0,40249.4);
            set("sim/graphics/view/world_matrix",12,18457.06+delta);
            set("sim/graphics/view/world_matrix",13,146.76-delta);
            set("sim/graphics/view/world_matrix",14,-40235.4+delta);
            project();
            check(projection_row()==near_projection,"distant origin and rounded world matrix leave aircraft hit map unchanged");
            check(cursor(1,centre.first,centre.second)==1,"stationary cursor remains inside the aircraft ND at distant origin");
        }
        set("sim/flightmodel/position/local_x",0,0);set("sim/flightmodel/position/local_y",0,0);set("sim/flightmodel/position/local_z",0,0);
        set("sim/graphics/view/world_matrix",12,.36);set("sim/graphics/view/world_matrix",13,-1.45);set("sim/graphics/view/world_matrix",14,14);
        set("sim/graphics/view/acf_matrix",12,.37);project();
        check(projection_row()!=near_projection,"real camera translation still updates ND hit map immediately");
        set("sim/graphics/view/acf_matrix",12,.36);project();
        check(cursor(1,0,0)==0,"leaving ND immediately restores OS cursor");
        const auto fo_button=screen(560,506,1);
        pointer(1,1,fo_button.first,fo_button.second,0);pointer(1,3,fo_button.first,fo_button.second,0);
        check(switch_state(true,true)&&report().find("ND_state side=1 enabled=1 blocked=\n")!=std::string::npos,"FO projected checkbox enables its own map independently");
        pointer(1,1,fo_button.first,fo_button.second,0);pointer(1,3,fo_button.first,fo_button.second,0);
        check(switch_state(true,true)&&report().find("ND_state side=1 enabled=0")!=std::string::npos,"unchecking FO leaves captain checkbox enabled");
        click(560,506);check(switch_state(true,false),"checkbox uncheck restores original map while leaving switch visible");
        auto button=screen(560,506),outside_button=screen(450,450);
        pointer(1,1,button.first,button.second,0);pointer(1,3,outside_button.first,outside_button.second,0);
        check(switch_state(true,false),"releasing outside checkbox cancels toggle");
        click(560,506);
        for(int range=1;range<=5;++range) {
            set("AirbusFBW/NDrangeCapt",0,range);advance(.1f);project();
            check(switch_state(false,true)&&pointer(1,1,button.first,button.second,0)==0,"non-10 NM hides switch and releases aircraft input even while checked");
        }
        set("AirbusFBW/NDrangeCapt",0,0);advance(.1f);project();
        check(switch_state(true,true),"return to 10 NM retains this session's checkbox state");
        for(int mode=0;mode<=4;++mode) {
            set("AirbusFBW/NDmodeCapt",0,mode);advance(.1f);project();
            check(switch_state(mode>=2,true),"OANS switch obeys supported NAV ARC PLAN display modes");
        }
        set("AirbusFBW/NDmodeCapt",0,2);advance(.1f);project();
        pointer(1,1,button.first,button.second,0);set("AirbusFBW/NDrangeCapt",0,1);advance(.1f);
        pointer(1,3,button.first,button.second,0);
        check(switch_state(false,true),"range change during checkbox press cancels capture and toggle");
        set("AirbusFBW/NDrangeCapt",0,0);advance(.1f);project();
        cmd("toliss_oans/capt_airport");check(focus()==1,"ND airport page takes keyboard focus on overlay window");
        set("AirbusFBW/NDmodeCapt",0,0);advance(.1f);
        check(focus()==-1,"leaving supported ND modes relinquishes airport keyboard focus");
        set("AirbusFBW/NDmodeCapt",0,2);advance(.1f);project();cmd("toliss_oans/capt_airport");
        for(char c:std::string("NO_SUCH"))if(c!='_')key(c);
        key(13);check(row(0).find("candidate=NONE")!=std::string::npos&&row(0).find("search_results=6")!=std::string::npos,"failed ND prefix clears candidate while preserving full airport list");
        for(int i=0;i<6;++i)key(8);
        click(180,38); // IATA radio
        click(132,91);auto letters=screen(130,40);wheel(1,letters.first,letters.second,100);click(125,39); // downward letter dropdown, B
        check(row(0).find("airport_mode=1 initial=B candidate=VABB")!=std::string::npos,"IATA B jumps to BOM candidate without loading map");
        check(row(0).find("confirmed_airport=ZTEST airport_modifying=1")!=std::string::npos,"browsing BOM keeps previously confirmed ZTEST details pending");
        check(row(0).find("search_results=4")!=std::string::npos&&row(0).find("airport_first=1 airport_dropdown=1")!=std::string::npos,"letter selection opens the complete downward airport list at B");
        auto list=screen(220,40);wheel(1,list.first,list.second,1);
        check(row(0).find("airport_first=0 airport_dropdown=1")!=std::string::npos&&row(0).find("range_nm=1")!=std::string::npos,"ND wheel scrolls back to A without zooming map");
        click(220,64);check(row(0).find("initial=A candidate=VAAA")!=std::string::npos,"selecting A airport confirms details and updates locator");
        click(282,91);wheel(1,list.first,list.second,-1);click(220,39);
        check(row(0).find("initial=C candidate=VCCC")!=std::string::npos,"same full list selects C and confirms its details");
        click(282,91);click(220,64);
        check(row(0).find("initial=B candidate=VABB")!=std::string::npos,"reopened dropdown retains B alongside C and T");
        check(row(0).find("confirmed_airport=VABB airport_modifying=0")!=std::string::npos,"selecting a dropdown row confirms details and clears editing colours");
        click(398,16);
        check(row(0).find("panel=2")!=std::string::npos,"DISPLAY ARPT stays on airport selection page");
        check(row(0).find("confirmed_airport=VABB airport_modifying=0")!=std::string::npos,"DISPLAY ARPT confirms candidate and clears pending state");
        for(int i=0;i<40;++i) {advance(.1f);Sleep(2);}
        cmd("toliss_oans/capt_airport");
        click(180,62);click(220,91); // ICAO radio, typing box
        for(char c:std::string("ZTEST"))key(c);
        check(row(0).find("query=ZTEST search_results=6")!=std::string::npos&&row(0).find("candidate=ZTEST")!=std::string::npos,"typed selector locates explicit 1302 alias without filtering list");
        click(398,16);check(focus()==-1,"display airport returns keyboard focus to simulator");
        check(row(0).find("confirmed_airport=ZTEST airport_modifying=0")!=std::string::npos,"confirming typed airport clears modification state");
        for(int i=0;i<40;++i) {advance(.1f);Sleep(2);}
        check(row(0).find("follow=0")!=std::string::npos,"DISPLAY ARPT holds airport reference instead of following aircraft");
        {
            const auto state=row(0);const auto at=state.find(" center=");
            double x=0,y=0;char comma=0;std::istringstream coords(state.substr(at+8));coords>>x>>comma>>y;
            check(at!=std::string::npos&&comma==','&&std::abs(x-962.976)<1&&std::abs(y-222.39)<1,"DISPLAY ARPT centers on datum coordinates in geometry projection");
        }
        check(session_log().find("LOADED ZTEST Test Airport")!=std::string::npos,"ND DISPLAY AIRPORT loads selected canonical source");
        set("AirbusFBW/TimeToAlign",0,480);advance(.2f);
        check(report().find("OANS_POSITION ready=0 pending=0 first_open=0 manual=1 airport=ZTEST ")!=std::string::npos&&
              report().find("TCAS traffic_icons=1 ")!=std::string::npos,
              "manually selected airport shows ADS-B traffic during alignment while ownship stays invalid");
        set("AirbusFBW/TimeToAlign",0,std::nan(""));advance(.2f);
        check(report().find("OANS_POSITION ready=0 ")!=std::string::npos,"invalid IRS alignment status cannot reveal positions");
        set("AirbusFBW/TimeToAlign",0,0);advance(.2f);
        check(report().find("OANS_POSITION ready=1 ")!=std::string::npos&&report().find("TCAS traffic_icons=1 ")!=std::string::npos,
              "aligned IRS restores position and traffic without replacing the manually selected airport");
        click(48,94); // collapse compact bottom panel
        cmd("toliss_oans/capt_range_in");cmd("toliss_oans/fo_range_out");
        check(row(0).find("range_nm=0.5")!=std::string::npos&&row(1).find("range_nm=2")!=std::string::npos,"captain/FO have independent map range");
        cmd("toliss_oans/capt_range_out");auto midpoint=screen(300,280);wheel(1,midpoint.first,midpoint.second,1);
        check(row(0).find("range_nm=0.5")!=std::string::npos,"mouse wheel over projected ND zooms its own map");cmd("toliss_oans/capt_range_out");
        auto menu_point=screen(120,250);pointer(1,1,menu_point.first,menu_point.second,1);pointer(1,3,menu_point.first,menu_point.second,1);
        check(row(0).find("menu=0")!=std::string::npos,"right click on projected ND does not open context menu");
        auto right_drag_start=screen(460,440),right_drag_end=screen(510,470);
        pointer(1,1,right_drag_start.first,right_drag_start.second,1);pointer(1,2,right_drag_end.first,right_drag_end.second,1);pointer(1,3,right_drag_end.first,right_drag_end.second,1);
        check(row(0).find("follow=0")!=std::string::npos&&row(0).find("menu=0")!=std::string::npos,"right drag pans the ND without opening a menu");
        click(120,250);check(row(0).find("menu=1")!=std::string::npos,"left click on projected ND opens context menu");
        click(200,234);check(row(0).find("crosses=1")!=std::string::npos,"first context row adds geographic cross");
        click(120,250);click(200,202);check(row(0).find("flags=1")!=std::string::npos,"second context row adds geographic flag");
        click(120,250);click(200,106);check(row(0).find("flags=0")!=std::string::npos,"fifth context row erases all flags");
        click(120,250);click(200,170);check(row(0).find("panel=1")!=std::string::npos,"third context row MENU opens the OANS panel");
        click(48,94);
        auto drag_start=screen(460,440),drag_end=screen(510,470);
        pointer(1,1,drag_start.first,drag_start.second,0);pointer(1,2,drag_end.first,drag_end.second,0);pointer(1,3,drag_end.first,drag_end.second,0);
        check(row(0).find("follow=0")!=std::string::npos&&row(0).find("menu=0")!=std::string::npos,"ND drag pans without opening a click menu");
        for(int i=0;i<100;++i)advance(.1f);
        check(row(0).find("follow=0")!=std::string::npos,"native ROSE keeps manual pan after release; no automatic return");
        set("AirbusFBW/NDmodeCapt",0,4);advance(.1f);
        pointer(1,1,drag_start.first,drag_start.second,0);pointer(1,2,drag_end.first,drag_end.second,0);pointer(1,3,drag_end.first,drag_end.second,0);
        for(int i=0;i<100;++i)advance(.1f);
        check(row(0).find("follow=0")!=std::string::npos,"native PLAN loop preserves released map pan");
        set("AirbusFBW/NDmodeCapt",0,3);advance(.1f);
        for(int i=0;i<100;++i)advance(.1f);
        check(row(0).find("follow=0")!=std::string::npos,"native ARC keeps manual pan across mode changes");
        cmd("toliss_oans/capt_center");check(row(0).find("follow=1")!=std::string::npos,"center command restores aircraft follow");
        cmd("toliss_oans/capt_panel");click(234,91);
        check(row(0).find("feature_dropdown=0 feature_initial_dropdown=0")!=std::string::npos,"disabled RWY left selector cannot open a menu");
        click(340,91);click(330,64); // right runway dropdown, first runway
        click(460,80);check(row(0).find("crosses=2")!=std::string::npos,"compact MAP DATA places a cross on selected runway");
        click(540,30);check(row(0).find("follow=0")!=std::string::npos,"compact CENTER MAP centers selected runway");
        click(460,30);check(row(0).find("feature_kind=0 feature_selection=0")!=std::string::npos&&writes()==0,"disabled LDG SHIFT does not alter selection or write aircraft controls");
        click(155,14);click(460,80);click(540,80);
        check(row(0).find("feature_kind=3 feature_selection=-1")!=std::string::npos&&row(0).find("crosses=2 flags=0")!=std::string::npos,"switching to OTHER cannot reuse stale runway selection for marks");
        click(234,91);click(223,64); // OTHER initial T, open TWR
        check(row(0).find("feature_kind=3 feature_selection=0")!=std::string::npos&&row(0).find("feature_dropdown=1")!=std::string::npos,"OTHER T selects tower and opens its right list");
        click(330,64);click(460,80);click(540,80);click(540,30);
        check(row(0).find("crosses=3 flags=1")!=std::string::npos&&row(0).find("follow=0")!=std::string::npos,"OTHER TWR supports cross flag and map centering through native clicks");
        {
            const auto state=row(0);const auto at=state.find(" center=");
            double x=0,y=0;char comma=0;std::istringstream coords(state.substr(at+8));coords>>x>>comma>>y;
            constexpr double pi=3.141592653589793;
            const double expected_x=.005*pi/180*6371000*std::cos(30*pi/180),expected_y=.0015*pi/180*6371000;
            check(at!=std::string::npos&&comma==','&&std::abs(x-expected_x)<1&&std::abs(y-expected_y)<1,"TWR centers on accepted tower viewpoint coordinates rather than ARP or runway");
        }
        click(155,62);click(340,91);click(330,64); // TWY A1
        click(540,80);check(row(0).find("flags=2")!=std::string::npos,"TWY remains selectable and ADD FLAG uses its geographic point");
        click(155,38);click(234,91);click(223,39); // STAND, initial B
        check(row(0).find("feature_kind=2 feature_selection=2")!=std::string::npos&&row(0).find("feature_dropdown=1")!=std::string::npos,"STAND left initial opens full feature list at B");
        auto feature_wheel=screen(330,39);wheel(1,feature_wheel.first,feature_wheel.second,1);
        check(row(0).find("feature_first=1")!=std::string::npos&&row(0).find("range_nm=1")!=std::string::npos,"MAP DATA wheel scrolls feature rows without map zoom");
        click(330,64);click(540,30);
        check(row(0).find("feature_kind=2 feature_selection=1")!=std::string::npos&&row(0).find("follow=0")!=std::string::npos,"STAND can scroll back to A2 and center it");
        check(report().find("OANS_DATABASE cycle=2701 validity=21/JAN-18/FEB")!=std::string::npos,"plugin reads cycle_info from host simulator root instead of hardcoding user sample");
        click(48,13);check(row(0).find("panel=3")!=std::string::npos,"bottom-left STATUS tab opens compact database panel");
        click(48,39);check(row(0).find("panel=2")!=std::string::npos,"bottom-left STATUS menu opens ARPT SEL");
        click(48,13);click(48,94);check(row(0).find("panel=0")!=std::string::npos,"STATUS collapses from above the bottom-left tabs");
        click(48,94);check(row(0).find("panel=3")!=std::string::npos,"same bottom-left control reopens remembered STATUS page");
        cmd("toliss_oans/capt_panel");
        check(writes()==0,"all OANS operations make zero aircraft DataRef writes");
        // Exercise the actual native advisory loop against the fixture runway,
        // without changing the user's simulator or loading a GL context here.
        set("sim/flightmodel2/gear/on_ground",1,1);set("sim/flightmodel/position/groundspeed",0,10);
        set("sim/flightmodel/position/psi",0,0);
        set("sim/flightmodel/position/latitude",0,30-150.0/6371000*180/3.141592653589793);
        set("sim/flightmodel/position/longitude",0,120.012);
        for(int i=0;i<110;++i)advance(.1f);
        check(report().find("RWY_AHEAD runway=09-27")!=std::string::npos,"native advisory predicts the runway independently of map follow state");
        set("sim/flightmodel/position/psi",0,180);
        set("sim/flightmodel/position/latitude",0,30+150.0/6371000*180/3.141592653589793);advance(.1f);
        check(report().find("RWY_AHEAD runway=27-09")!=std::string::npos,"live opposite-side approach reverses displayed ends");
        set("sim/flightmodel/position/psi",0,0);
        set("sim/flightmodel/position/latitude",0,30-150.0/6371000*180/3.141592653589793);advance(.1f);
        set("AirbusFBW/NDrangeCapt",0,1);advance(.1f);
        check(report().find("RWY_AHEAD_display side=0 nd=1 pfd=1")!=std::string::npos,"native ND and PFD advisory survives physical map range change");
        set("AirbusFBW/NDmodeCapt",0,0);advance(.1f);
        check(report().find("RWY_AHEAD_display side=0 nd=0 pfd=1")!=std::string::npos,"PFD advisory remains available outside map modes");
        set("AirbusFBW/DUBrightness",0,0);advance(.1f);
        check(report().find("RWY_AHEAD_display side=0 nd=0 pfd=0")!=std::string::npos,"PFD brightness gate");
        set("AirbusFBW/DUBrightness",0,1);set("AirbusFBW/NDmodeCapt",0,2);set("AirbusFBW/NDrangeCapt",0,0);
        set("sim/flightmodel/position/groundspeed",0,0);advance(.1f);
        check(report().find(" count=0 ")!=std::string::npos,"native stopped aircraft immediately clears warning");
        check(writes()==0,"runway advisory only reads aircraft DataRefs");
        function<void(*)()>(mock,"MockInitFms")();
        auto fms_page=function<const char*(*)()>(mock,"MockFmsPage");
        auto fms_plan=function<void(*)(const char*,const char*)>(mock,"MockFmsPlan");
        set("AirbusFBW/ECAMFlightPhase",0,1);
        for(int i=0;i<85;++i)advance(.1f);
        check(report().find("observed=0")!=std::string::npos&&std::string(fms_page())=="IDENT","phase one suppresses initial automatic MCDU flipping");
        check(report().find("MCDU_MAJOR mcdu1=DATA mcdu2=DATA")!=std::string::npos,"both major pages initially DATA");
        cmd("AirbusFBW/MCDU1Init");cmd("AirbusFBW/MCDU1LSK1R");
        for(int i=0;i<85;++i)advance(.1f);
        check(std::string(fms_page())=="IDENT"&&report().find("saved_page=A/C STATUS")!=std::string::npos,
              "MCDU1 trigger restores untouched MCDU2 through DATA and LSK4L");
        menu_click(12);menu_click(12);
        set("AirbusFBW/ECAMFlightPhase",0,2);
        for(int i=0;i<85;++i)advance(.1f);
        check(std::string(fms_page())=="IDENT"&&report().find("confirmations=1 complete=0")!=std::string::npos,
              "first automatic capture restores untouched MCDU2 to A/C STATUS");
        // Automatic retries are spaced 60 seconds apart. Wait for completion
        // rather than assuming the older 30-second retry interval.
        for(int i=0;i<700;++i) {advance(.1f);if(i%10==0&&report().find("confirmations=2 complete=1")!=std::string::npos)break;}
        check(std::string(fms_page())=="IDENT"&&report().find("confirmations=2 complete=1")!=std::string::npos,
              "internal sampling commands never mark MCDU2 as operated");
        menu_click(12);menu_click(12);
        set("AirbusFBW/ECAMFlightPhase",0,1);
        advance(.1f); // Rebind the observers after re-enabling sampling.
        cmd("AirbusFBW/MCDU2KeyA");cmd("AirbusFBW/MCDU1LSK1R");
        for(int i=0;i<85;++i)advance(.1f);
        check(std::string(fms_page())=="DATA"&&report().find("saved_page=DATA")!=std::string::npos,
              "ordinary MCDU2 key ends untouched restoration without changing its DATA parent");
        const std::pair<const char*,const char*> major_keys[]={
            {"DirTo","DIR"},{"Prog","PROG"},{"Perf","PERF"},{"Init","INIT"},{"Data","DATA"},
            {"Fpln","F-PLN"},{"RadNav","RAD NAV"},{"FuelPred","FUEL PRED"},
            {"SecFpln","SEC F-PLN"},{"ATC","ATC COMM"},{"Menu","MCDU MENU"},{"Airport","AIR PORT"}};
        for(int unit:{1,2})for(const auto& item:major_keys) {
            cmd(("AirbusFBW/MCDU"+std::to_string(unit)+item.first).c_str());
            const std::string expected=unit==1?std::string("MCDU_MAJOR mcdu1=")+item.second+" mcdu2=DATA":
                std::string("MCDU_MAJOR mcdu1=AIR PORT mcdu2=")+item.second+"\n";
            check(report().find(expected)!=std::string::npos,"all major keys update only their own MCDU");
        }
        cmd("AirbusFBW/MCDU1Perf");cmd("AirbusFBW/MCDU2Data");
        cmd("AirbusFBW/MCDU1LSK1L");cmd("AirbusFBW/MCDU2LSK4L");
        cmd("AirbusFBW/MCDU1SlewRight");cmd("AirbusFBW/MCDU2KeyA");
        check(report().find("MCDU_MAJOR mcdu1=PERF mcdu2=DATA")!=std::string::npos,"LSK, arrow and character keys preserve major pages");
        cmd("AirbusFBW/MCDU1Init");cmd("AirbusFBW/MCDU1LSK1R");
        for(int i=0;i<85;++i)advance(.1f);
        check(report().find("confirmations=1 complete=1")!=std::string::npos,"one full selected-key capture completes automatic session in phase one");
        set("AirbusFBW/ECAMFlightPhase",0,2);
        for(int i=0;i<400;++i)advance(.1f);
        check(report().find("ecam_phase=1 confirmations=1 complete=1")!=std::string::npos,"crew completion stops phase polling and automatic recapture");
        cmd("AirbusFBW/MCDU1Perf");
        // Start a fresh capture
        // session so the existing automatic-confirmation checks stay isolated.
        menu_click(12);menu_click(12);
        set("AirbusFBW/ECAMFlightPhase",0,2);
        for(int i=0;i<85;++i)advance(.1f);
        auto fms_report=report();
        check(fms_report.find("origin=ZUCK departure_runway=02L destination=ZUUU arrival_runway=02L")!=std::string::npos,"actual .xpl reads byte DataRefs and publishes FMS endpoints");
        check(row(0).find("not_in_plan=1")!=std::string::npos,"native selected ZTEST is outside observed ZUCK to ZUUU plan");
        check(std::string(fms_page())=="DATA","actual .xpl restores DATA parent from A/C STATUS subpage");
        check(fms_report.find("alternate=ZALT ")!=std::string::npos,"actual .xpl reads alternate from FUEL PRED");
        check(fms_report.find("confirmations=1 complete=0")!=std::string::npos,"first automatic capture requires a second airport confirmation");
        check(fms_report.find("MCDU_MAJOR mcdu1=PERF mcdu2=DATA")!=std::string::npos,"sampler navigation does not overwrite crew major pages");
        for(int i=0;i<700;++i) {advance(.1f);if(i%10==0&&report().find("confirmations=2 complete=1")!=std::string::npos)break;}
        check(report().find("confirmations=2 complete=1")!=std::string::npos,"second matching airport pair stops automatic capture");
        menu_click(12);menu_click(12);
        set("AirbusFBW/ECAMFlightPhase",0,5);
        for(int i=0;i<85;++i)advance(.1f);
        check(std::string(fms_page())=="DATA"&&report().find("observed=0")!=std::string::npos&&
              report().find("ecam_phase=5 confirmations=0 complete=0")!=std::string::npos,
              "phase five blocks automatic detection without latching a successful capture");
        cmd("AirbusFBW/MCDU1Init");cmd("AirbusFBW/MCDU1LSK1R");
        for(int i=0;i<49;++i)advance(.1f);
        check(std::string(fms_page())=="DATA"&&report().find("observed=0")!=std::string::npos,
              "selected-key capture waits five seconds even when phase five blocks automatic capture");
        for(int i=0;i<40;++i)advance(.1f);
        check(report().find("origin=ZUCK departure_runway=02L destination=ZUUU arrival_runway=02L")!=std::string::npos,
              "phase five still permits selected-key capture");
        auto fms_alternate=function<void(*)(const char*)>(mock,"MockFmsAlternate");
        project();cmd("toliss_oans/capt_airport");click(550,19);
        for(int i=0;i<40;++i) {advance(.1f);Sleep(2);}
        check(row(0).find("candidate=ZALT")!=std::string::npos&&session_log().find("LOADED ZALT Alternate Test Airport")!=std::string::npos,
              "native ALTN shortcut loads sampled alternate from airport database");
        check(row(0).find("panel=2")!=std::string::npos&&row(0).find("airport_modifying=0")!=std::string::npos,"FMS airport shortcut stays on airport page with confirmed selection");
        check(row(0).find("not_in_plan=0")!=std::string::npos,"selected alternate belongs to active plan in actual .xpl");
        fms_alternate("----");for(int i=0;i<330;++i)advance(.1f);
        check(report().find("alternate=ZALT ")!=std::string::npos,"completed automatic capture does not poll later silent plan changes");
        cmd("AirbusFBW/MCDU2Fpln");cmd("AirbusFBW/MCDU2LSK6R");for(int i=0;i<85;++i)advance(.1f);
        check(report().find("alternate= saved_page=")!=std::string::npos&&row(0).find("not_in_plan=1")!=std::string::npos,"cleared alternate removes active plan membership");
        fms_alternate("ZALT");
        cmd("toliss_oans/capt_airport");click(220,91);
        for(char c:std::string("ZTEST"))key(c);
        click(398,16);for(int i=0;i<40;++i) {advance(.1f);Sleep(2);}
        fms_plan("ZUCK","ZUUU");cmd("AirbusFBW/MCDU2Fpln");cmd("AirbusFBW/MCDU2LSK6R");for(int i=0;i<85;++i)advance(.1f);
        check(report().find("origin=ZUCK departure_runway= destination=ZUUU arrival_runway=")!=std::string::npos,"native next sample clears deselected runways");
        fms_plan("PPOS","");cmd("AirbusFBW/MCDU1Init");cmd("AirbusFBW/MCDU1LSK1R");for(int i=0;i<85;++i)advance(.1f);
        check(report().find("plan_status=NO FLIGHT PLAN origin= departure_runway= destination= arrival_runway=")!=std::string::npos,"native PPOS sample clears old airports");
        fms_plan("ZUCK02L","ZUUU02L");
        cmd("AirbusFBW/MCDU2FuelPred");advance(.1f);
        check(std::string(fms_page())=="FUEL","crew input passes through sampler observer");
        for(int i=0;i<44;++i)advance(.1f);
        check(std::string(fms_page())=="FUEL"&&report().find("plan_status=NO FLIGHT PLAN")!=std::string::npos,"ordinary page key defers the pending crew retry for five seconds");
        for(int i=0;i<40;++i)advance(.1f);
        check(std::string(fms_page())=="FUEL"&&report().find("plan_status=CONNECTED")!=std::string::npos&&
              session_log().find("trigger detect INIT LSK1R retry 1 success")!=std::string::npos,
              "failed selected-key capture retries with its original page and key source");
        fms_alternate("ZBAA");
        cmd("AirbusFBW/MCDU2LSK6R");for(int i=0;i<70;++i)advance(.1f);
        check(report().find("alternate=ZALT ")!=std::string::npos,"R6 outside F-PLN does not request sampling after retry success");
        fms_alternate("ZALT");
        cmd("AirbusFBW/MCDU1Fpln");cmd("AirbusFBW/MCDU1LSK6R");
        for(int i=0;i<49;++i)advance(.1f);
        check(std::string(fms_page())=="FUEL","selected key waits five seconds before navigating MCDU2");
        for(int i=0;i<40;++i)advance(.1f);
        check(std::string(fms_page())=="FUEL"&&report().find("origin=ZUCK departure_runway=02L destination=ZUUU arrival_runway=02L")!=std::string::npos,"sampling resumes and restores crew-selected FUEL PRED");
        for(int unit:{1,2}) {
            const char* alternate=unit==1?"ZBAA":"ZALT";
            fms_alternate(alternate);
            cmd(("AirbusFBW/MCDU"+std::to_string(unit)+"Init").c_str());
            cmd(("AirbusFBW/MCDU"+std::to_string(unit)+"LSK2L").c_str());
            for(int i=0;i<85;++i)advance(.1f);
            check(report().find(std::string("alternate=")+alternate+" saved_page=")!=std::string::npos,"INIT L2 on either MCDU refreshes alternate after automatic capture completed");
        }
        const std::pair<const char*,const char*> parent_roots[]={
            {"Data","DATA"},{"DirTo","DIR"},{"Init","INIT"},{"Fpln","FPLN"},
            {"FuelPred","FUEL"},{"RadNav","RADNAV"},{"Menu","MENU"},{"SecFpln","SEC"},
            {"Perf","PERF"},{"Prog","PROG"},{"ATC","ATC"},{"Airport","FPLN"}};
        cmd("AirbusFBW/MCDU1Fpln");
        for(const auto& parent:parent_roots) {
            cmd((std::string("AirbusFBW/MCDU2")+parent.first).c_str());
            function<void(*)()>(mock,"MockFmsSubpage")();
            cmd("AirbusFBW/MCDU1LSK6R");
            for(int i=0;i<85;++i)advance(.1f);
            check(std::string(fms_page())==parent.second&&report().find("status=CONNECTED")!=std::string::npos,
                  "actual .xpl restores tracked MCDU2 parent from unknown subpage for every major key");
        }
        check(writes()==0,"FMS page sampling makes zero aircraft DataRef writes");
        menu_click(1);check(focus()==2,"Test menu opens independent input window and keyboard focus");
        int tl,tt,tr,tb;function<void(*)(int,int*,int*,int*,int*)>(mock,"MockWindowRect")(2,&tl,&tt,&tr,&tb);
        auto test_click=[&](int x,int y){pointer(2,1,tl+x,tb+y,0);pointer(2,3,tl+x,tb+y,0);};
        auto type=[&](const std::string& s){for(char c:s)key(c);};
        check(tt-tb==580,"Test window has space for two-axis OANS offset controls");
        test_click(490,366);type("-20");key(13);
        check(report().find("bounds=534,515,598,537 vertical_offset_px=-20")!=std::string::npos,"negative offset moves switch hit bounds up by twenty ND pixels");
        type("900");key(13);
        check(report().find("bounds=534,-105,598,-83 vertical_offset_px=600")!=std::string::npos,"offset input clamps at six hundred for wide-area diagnosis");
        type("-");key(13);
        check(report().find("vertical_offset_px=600")!=std::string::npos,"incomplete signed input preserves last valid offset");
        test_click(180,366);
        check(report().find("vertical_offset_px=-20")!=std::string::npos,"slider left endpoint selects minus twenty");
        pointer(2,1,tl+306,tb+366,0);pointer(2,2,tl+600,tb+366,0);pointer(2,3,tl+600,tb+366,0);
        check(report().find("vertical_offset_px=20")!=std::string::npos,"slider drag beyond right endpoint clamps and releases capture");
        test_click(490,366);type("7");key(13);
        check(report().find("bounds=534,488,598,510 vertical_offset_px=7")!=std::string::npos,"input selects an intermediate integer offset");
        const bool checked_before_offset_click=report().find("OANS_switch side=0 visible=1 checked=1")!=std::string::npos;
        click(560,499);
        check(report().find("OANS_switch side=0 visible=1 checked="+std::to_string(!checked_before_offset_click))!=std::string::npos,"shifted switch responds at its new projected click position");
        click(560,499);
        test_click(490,496);type("-300");key(13);
        check(report().find("bounds=234,488,298,510 vertical_offset_px=7 horizontal_offset_px=-300")!=std::string::npos,"horizontal input moves both switch bounds while retaining vertical offset");
        const bool checked_before_xy_click=report().find("OANS_switch side=0 visible=1 checked=1")!=std::string::npos;
        click(260,499);
        check(report().find("OANS_switch side=0 visible=1 checked="+std::to_string(!checked_before_xy_click))!=std::string::npos,"two-axis shifted button uses matching projected hit bounds");
        click(260,499);
        test_click(60,448);test_click(260,448);
        check(report().find("bounds=209,513,273,535 vertical_offset_px=-18 horizontal_offset_px=-325")!=std::string::npos,"left and up controls move twenty-five pixels");
        test_click(160,448);test_click(360,448);
        check(report().find("bounds=234,488,298,510 vertical_offset_px=7 horizontal_offset_px=-300")!=std::string::npos,"right and down controls reverse movement");
        test_click(490,496);type("-999");key(13);
        check(report().find("horizontal_offset_px=-600")!=std::string::npos,"horizontal input clamps negative offsets");
        test_click(490,316);
        check(report().find("bounds=534,495,598,517 vertical_offset_px=0 horizontal_offset_px=0")!=std::string::npos,"reset restores both axes to the installed baseline layout");
        test_click(250,246);type("ZTEST");key(9);type("9");key(13);
        check(report().find("RWY_AHEAD_TEST active=1 pending=0 steady=1")!=std::string::npos&&report().find("source=TEST")!=std::string::npos,"entered airport and runway enable test while aircraft stopped");
        for(int i=0;i<400;++i)advance(.1f);
        check(report().find("RWY_AHEAD runway=09-27")!=std::string::npos&&report().find(" blink=1 source=TEST")!=std::string::npos,"test stays visible beyond thirty seconds without motion or blinking");
        set("sim/flightmodel/position/psi",0,180);advance(.1f);
        check(report().find("RWY_AHEAD runway=27-09")!=std::string::npos&&report().find("source=TEST")!=std::string::npos,"forced test uses the same aircraft-relative endpoint order");
        set("sim/flightmodel/position/psi",0,0);advance(.1f);
        set("sim/time/paused",0,1);advance(2);
        check(report().find(" blink=1 source=TEST")!=std::string::npos,"test screenshot stays lit in pause");set("sim/time/paused",0,0);
        key(27);check(focus()==-1&&report().find("source=TEST")!=std::string::npos,"Escape closes dialog while retaining test warning");
        menu_click(1);test_click(60,145);
        check(report().find("active=1 pending=0 steady=0")!=std::string::npos,"test can optionally enable normal flashing");
        test_click(250,96);advance(.1f);
        check(report().find("RWY_AHEAD_TEST active=0 pending=0")!=std::string::npos&&report().find(" count=0 ")!=std::string::npos,"Stop test restores live detector instead of latching warning");
        test_click(250,246);type("NOAPT");key(13);
        check(report().find("AIRPORT NOT FOUND: NOAPT")!=std::string::npos&&report().find("source=TEST")==std::string::npos,"bad airport cannot activate fake warning");
        test_click(250,246);type("ZTEST");key(9);type("08");key(13);
        check(report().find("RUNWAY 08 NOT IN ZTEST")!=std::string::npos&&report().find("source=TEST")==std::string::npos,"bad runway reports selected airport mismatch");
        test_click(250,194);type("09");key(13);test_click(430,96);
        check(focus()==-1&&report().find("source=TEST")!=std::string::npos,"Close button releases focus but keeps test active");
        menu_click(1);test_click(250,246);type("ZALT");key(9);type("18");key(13);
        check(report().find("active=0 pending=1")!=std::string::npos,"test requests named airport in file worker");
        for(int i=0;i<100;++i) {advance(.1f);Sleep(3);}
        check(report().find("RWY_AHEAD airport=ZALT")!=std::string::npos&&report().find("RWY_AHEAD runway=18-36")!=std::string::npos,"test activates selected airport after asynchronous geometry load");
        function<void(*)(int)>(mock,"MockHideWindow")(2);advance(.1f);
        check(focus()==-1&&report().find("source=TEST")!=std::string::npos,"native window close releases typing focus and retains test");
        menu_click(1);test_click(250,246);type("ZTEST");key(9);type("09");key(13);test_click(250,96);
        for(int i=0;i<100;++i) {advance(.1f);Sleep(3);}
        check(report().find("active=0 pending=0")!=std::string::npos&&report().find("source=TEST")==std::string::npos,"stopping pending load cannot reactivate test after worker finishes");
        test_click(250,194);type("09");key(13);test_click(430,96);
        check(writes()==0,"manual test makes zero aircraft DataRef writes");
        menu_click(12);advance(.1f);
        check(!menu_checked(12),"disable automatic MCDU2 sampling");
        cmd("AirbusFBW/MCDU1Init");cmd("AirbusFBW/MCDU2Prog");
        check(report().find("MCDU_MAJOR mcdu1=INIT mcdu2=PROG")!=std::string::npos,"major tracking works with automatic sampling disabled");
        menu_click(12);advance(.1f);
        check(report().find("MCDU_MAJOR mcdu1=INIT mcdu2=PROG")!=std::string::npos,"sampling setting preserves major pages");
        // SDK message 103 positions the user's aircraft at a new airport.
        set("AirbusFBW/CaptMAPAvail",0,0);set("AirbusFBW/CoMAPAvail",0,0);
        message(0,103,nullptr);
        check(report().find("MCDU_MAJOR mcdu1=INIT mcdu2=PROG")!=std::string::npos,"airport load preserves major pages");
        for(int i=0;i<60;++i) {advance(.1f);Sleep(2);}
        check(report().find("OANS_POSITION ready=0 pending=1 first_open=1 manual=0 airport=NONE requested=\n")!=std::string::npos,
              "new-airport message clears previous manual selection and rejects late map loads while IRS is unavailable");
        set("sim/flightmodel/position/latitude",0,30.2);
        set("AirbusFBW/CaptMAPAvail",0,1);set("AirbusFBW/CoMAPAvail",0,1);
        set("sim/flightmodel/position/latitude",0,std::nan(""));advance(.1f);
        check(report().find("OANS_POSITION ready=0 pending=1")!=std::string::npos,
              "MAP AVAIL with invalid coordinates does not consume first position confirmation");
        set("sim/flightmodel/position/latitude",0,30.2);
        for(int i=0;i<80;++i) {advance(.1f);Sleep(2);}
        check(report().find("OANS_POSITION ready=1 pending=0 first_open=1 manual=0 airport=ZNEAR ")!=std::string::npos,
              "delayed IRS availability after a new flight selects the new airport instead of the last session");
        set("AirbusFBW/CaptMAPAvail",0,0);set("AirbusFBW/CoMAPAvail",0,0);advance(.1f);
        check(report().find("OANS_POSITION ready=0 pending=1 first_open=1 manual=0 airport=NONE requested=\n")!=std::string::npos,
              "losing position clears the automatic airport and rearms confirmation");
        set("AirbusFBW/CaptMAPAvail",0,1);set("AirbusFBW/CoMAPAvail",0,1);
        for(int i=0;i<80;++i) {advance(.1f);Sleep(2);}
        message(0,106,nullptr);message(0,102,nullptr);
        check(report().find("MCDU_MAJOR mcdu1=DATA mcdu2=DATA")!=std::string::npos,"aircraft reload resets both major pages to DATA");
        check(report().find("pending=1 first_open=1 manual=0 airport=NONE requested=\n")!=std::string::npos,
              "user-aircraft unload and reload clear the previous map and rearm first opening");
        for(int i=0;i<80;++i) {advance(.1f);Sleep(2);}
        cmd("toliss_oans/capt_airport");check(focus()==1,"ND search can acquire focus during session");
        disable();
        check(report().find("RWY_AHEAD_TEST active=0 pending=0")!=std::string::npos,"disable clears forced test warning");
        cmd("toliss_oans/capt_airport");
        check(focus()==-1,"disable returns keyboard focus and prevents disabled commands reacquiring it");
        check(report().find("BrakePedalInput")==std::string::npos&&report().find("speedbrake1_deg")==std::string::npos,"OANS does not bind retired BTV inputs");
        stop();shutdown=nullptr;check(resources()==0,"stop unregisters all callbacks, commands and windows");
        check(FreeLibrary(plugin)!=0,"unload native plugin after worker shutdown");plugin=nullptr;FreeLibrary(mock);mock=nullptr;
        std::cout<<"PASS "<<checks<<" native loader/lifecycle checks (MOCK SDK ONLY)\n";return 0;
    } catch(const std::exception& e) {
        const bool expected=failure_test&&std::string(e.what())=="EXPECTED_CLEANUP_REGRESSION";
        if(!expected)std::cerr<<"FAIL "<<e.what()<<" WindowsError="<<GetLastError()<<'\n';
        if(shutdown)shutdown();
        const bool clean=mock&&function<int(*)()>(mock,"MockResources")()==0;
        if(plugin)FreeLibrary(plugin);
        if(mock)FreeLibrary(mock);
        if(expected&&clean) {std::cout<<"PASS controlled test failure stops worker and releases SDK resources\n";return 0;}
        return 1;
    }
}
