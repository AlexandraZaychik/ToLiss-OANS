// Loads the real plugin into a private mock SDK process; never attaches to X-Plane.
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include "test_support.hpp"
#include "menu_layout.hpp"
template<class T>T function(HMODULE module,const char* name){auto p=GetProcAddress(module,name);verify(p!=nullptr,std::string("missing export ")+name);return reinterpret_cast<T>(p);}
int main(int argc,char** argv){
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
 if(argc!=5&&argc!=6)return 2;
 const std::string mode=argc==6?argv[5]:"";
 HMODULE mock=nullptr,plugin=nullptr;void(*stop)()=nullptr;
 try{
  mock=LoadLibraryExW(std::filesystem::u8path(argv[1]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);verify(mock!=nullptr,"load mock SDK");
  auto configure=function<void(*)(const char*,const char*,const char*)>(mock,"MockConfigure");configure(argv[2],argv[3],argv[4]);
  function<void(*)()>(mock,"MockInitFms")();
  auto set=function<void(*)(const char*,int,double)>(mock,"MockSet");
  auto advance=function<void(*)(float)>(mock,"MockAdvance");
  auto cmd=function<void(*)(const char*)>(mock,"MockCommand");
  auto focus=function<int(*)()>(mock,"MockFocus");auto key=function<void(*)(int)>(mock,"MockKey");
  auto resources=function<int(*)()>(mock,"MockResources");auto writes=function<int(*)()>(mock,"MockSetters");
  auto observers=function<int(*)(const char*)>(mock,"MockCommandObservers");
  auto menu_click=function<void(*)(int)>(mock,"MockMenuClick");auto menu_checked=function<int(*)(int)>(mock,"MockMenuChecked");
  auto menu=[&](const char* id){for(size_t i=0;i<std::size(oans::menu_layout);++i)if(std::string(oans::menu_layout[i].id)==id)return int(i);throw std::runtime_error("missing menu ID");};
  const auto package=std::filesystem::u8path(argv[2]).parent_path().parent_path();
  auto read=[](const auto& path){std::ifstream f(path);std::ostringstream out;out<<f.rdbuf();return out.str();};
  auto report=[&](){cmd("toliss_oans/diagnostics");return read(package/"interfaces.txt");};
  auto wait=[&](int frames){for(int i=0;i<frames;++i){advance(.1f);Sleep(2);}};
  plugin=LoadLibraryExW(std::filesystem::u8path(argv[2]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);verify(plugin!=nullptr,"load actual plugin");
  stop=function<void(*)()>(plugin,"XPluginStop");auto disable=function<void(*)()>(plugin,"XPluginDisable");auto enable=function<int(*)()>(plugin,"XPluginEnable");
  function<void(*)(void(*)())>(mock,"MockSetDisableCallback")(disable);
  auto message=function<void(*)(int,int,void*)>(plugin,"XPluginReceiveMessage");
  char name[256],sig[256],desc[256];
  auto start=function<int(*)(char*,char*,char*)>(plugin,"XPluginStart");
  auto set_icao=function<void(*)(const char*)>(mock,"MockAircraftIcao");
  if(mode=="a359"||mode=="a35k") {
   set_icao(mode=="a359"?"A359":"A35K");
   verify(start(name,sig,desc)==0,"A350 startup rejected");stop=nullptr;
   verify(resources()==0,"blocked startup registers no resources");
   FreeLibrary(plugin);FreeLibrary(mock);return 0;
  }
  scenario((mode.empty()||mode=="language")?"native/cold-start-position-confirmation":"native/cache/"+mode);
  verify(start(name,sig,desc)==1&&enable()==1,"start and enable");
  if(mode=="a350-switch") {
   set_icao("A359");message(0,102,nullptr);verify(enable()==0,"A359 cannot be enabled after switching");
   set_icao("A320");verify(enable()==1,"non-A350 can be enabled");
   set_icao("A35K");advance(.1f);verify(enable()==0,"A35K detected by flight loop");
   stop();stop=nullptr;verify(resources()==0,"stop releases resources");
   FreeLibrary(plugin);FreeLibrary(mock);return 0;
  }
  verify(read(package/"interfaces.txt").find("AIRCRAFT_IDENTIFICATION state=WAITING")!=std::string::npos,"identification is deferred beyond plugin start");
  if(mode.empty()||mode=="language") {
   const auto original=read(package/"config.ini");
   const auto label=function<const char*(*)(int)>(mock,"MockMenuLabel");
   const auto set_language=function<void(*)(int)>(mock,"MockSetLanguage");
   auto config_language=[&](const std::string& value) {
    std::istringstream input(original);std::string line,contents;
    while(std::getline(input,line))if(line.rfind("language=",0)!=0)contents+=line+"\n";
    std::ofstream out(package/"config.ini");out<<contents<<value;out.close();cmd("toliss_oans/reload");
   };
   for(const auto value:{"", "language=auto\n", "language=invalid\n"}) {
    for(int language=0;language<=11;++language) {
     set_language(language);config_language(value);
     verify(std::string(label(menu("settings")))==(language==10?"设置":"Setting"),"automatic language follows Chinese only; unknown and all others use English");
     verify(!menu_checked(menu("language_english"))&&!menu_checked(menu("language_chinese")),"automatic language leaves manual choices unselected");
    }
   }
   set_language(1);config_language("language=auto\n");
   menu_click(menu("language_chinese"));
   verify(read(package/"config.ini").find("language=chinese")!=std::string::npos,"Chinese choice persisted");
   verify(std::string(label(menu("settings")))=="设置"&&menu_checked(menu("language_chinese"))&&!menu_checked(menu("language_english")),"Chinese choice updates submenu labels and checks immediately");
   cmd("toliss_oans/reload");
   verify(std::string(label(menu("settings")))=="设置"&&menu_checked(menu("language_chinese")),"Chinese choice survives reload over English simulator");
   set_language(10);menu_click(menu("language_english"));cmd("toliss_oans/reload");
   verify(std::string(label(menu("settings")))=="Setting"&&menu_checked(menu("language_english"))&&!menu_checked(menu("language_chinese")),"English overrides Chinese simulator and survives reload");
   verify(read(package/"config.ini").find("language=english")!=std::string::npos,"English choice persisted");
   verify(std::string(label(menu("language_english")))=="English"&&std::string(label(menu("language_chinese")))=="中文","language choice names remain recognizable");
   std::ofstream restore(package/"config.ini");restore<<original;restore.close();
   set_language(1);cmd("toliss_oans/reload");
   if(mode=="language") {
    disable();stop();stop=nullptr;verify(resources()==0,"language test releases resources");
    FreeLibrary(plugin);plugin=nullptr;FreeLibrary(mock);mock=nullptr;return 0;
   }
  }
  set("AirbusFBW/CaptMAPAvail",0,0);set("AirbusFBW/CoMAPAvail",0,0);set("AirbusFBW/TimeToAlign",0,480);wait(60);
  verify(report().find("OANS_POSITION ready=0 pending=1")!=std::string::npos,"cold cockpit cannot choose airport from simulator position");
  set("AirbusFBW/TimeToAlign",0,0);set("AirbusFBW/CaptMAPAvail",0,1);set("AirbusFBW/CoMAPAvail",0,1);
  set("sim/flightmodel/position/latitude",0,30);set("sim/flightmodel/position/longitude",0,120.012);cmd("toliss_oans/nearest");wait(100);
  verify(report().find("airport=ZTEST ")!=std::string::npos,"confirmed position loads fixture airport");
  const auto expected=mode=="reuse"?"Building source: reused DSF cache":mode=="osm"?"Building fallback: reused cached OSM data":"Building source: extracted runtime scenery DSF:";
  verify(read(package/"diagnostics.log").find(expected)!=std::string::npos,"actual worker selects expected building source");
  if(!mode.empty()){
   verify(read(package/"diagnostics.log").find("downloaded and cached OSM")==std::string::npos,"offline cache workflow never downloads");
   disable();stop();stop=nullptr;verify(resources()==0,"cache workflow stops workers and callbacks");
   FreeLibrary(plugin);plugin=nullptr;FreeLibrary(mock);mock=nullptr;return 0;
  }

  scenario("native/mcdu-capture-restoration-no-writes");
  verify(menu_checked(menu("detail_medium")),"detail quality defaults to medium");
  const char* detail_ids[]={"detail_ultra","detail_max","detail_xhigh","detail_high","detail_medium","detail_low"};
  const char* detail_values[]={"ultra","max","xhigh","high","medium","low"};
  for(int quality=0;quality<6;++quality) {
   menu_click(menu(detail_ids[quality]));
   verify(read(package/"config.ini").find(std::string("detail_quality=")+detail_values[quality])!=std::string::npos,"detail quality is saved");
   cmd("toliss_oans/reload");
   for(int other=0;other<6;++other)verify(bool(menu_checked(menu(detail_ids[other])))==(other==quality),"detail quality is exclusive and survives reload");
  }
  menu_click(menu("detail_medium"));
  set("AirbusFBW/ECAMFlightPhase",0,2);
  if(!menu_checked(menu("mcdu2_full_open")))menu_click(menu("mcdu2_full_open"));
  auto fms_page=function<const char*(*)()>(mock,"MockFmsPage");
  const auto before=std::string(fms_page());wait(90);
  verify(report().find("origin=ZUCK departure_runway=02L destination=ZUUU arrival_runway=02L")!=std::string::npos&&std::string(fms_page())==before,"real byte DataRefs publish plan and restore original page");
  verify(writes()==0,"FMS navigation never writes aircraft controls");
  cmd("AirbusFBW/MCDU2Fpln");cmd("AirbusFBW/MCDU2LSK6R");wait(140);
  verify(read(package/"diagnostics.log").find("trigger detect F-PLN LSK6R success")!=std::string::npos,"full open retains existing key-triggered capture");
  cmd("AirbusFBW/MCDU2Data");cmd("AirbusFBW/MCDU2LSK4L");wait(60);

  set("sim/cockpit2/switches/beacon_on",0,0);
  set("sim/cockpit2/switches/landing_lights_switch",0,0);
  set("AirbusFBW/ECAMFlightPhase",0,6);
  menu_click(menu("mcdu2_with_light"));wait(60);
  verify(menu_checked(menu("mcdu2_with_light"))&&!menu_checked(menu("mcdu2_full_open"))&&!menu_checked(menu("mcdu2_full_close")),"three MCDU modes are mutually exclusive");
  verify(read(package/"config.ini").find("fms_mcdu2_sampling=2")!=std::string::npos,"light mode persists");
  cmd("toliss_oans/reload");wait(60);
  verify(menu_checked(menu("mcdu2_with_light"))&&report().find("FMS_LIGHT mode=1")!=std::string::npos,"light mode survives config reload");
  auto count_text=[](const std::string& s,const std::string& key){size_t n=0,pos=0;while((pos=s.find(key,pos))!=std::string::npos){++n;pos+=key.size();}return n;};
  const auto light_log=[&](){return read(package/"diagnostics.log");};
  verify(light_log().find("light detect")==std::string::npos,"no read until a rising edge");
  const auto automatic_before=count_text(light_log(),"auto detect try ");
  set("AirbusFBW/ECAMFlightPhase",0,2);wait(650);
  verify(count_text(light_log(),"auto detect try ")==automatic_before&&report().find("observed=0")!=std::string::npos,"With light disables phase/engine automatic capture");
  auto check_light_keys=[&](const std::string& prefix,const char* init_key,const char* fpln_key,const char* sec_key) {
    const auto direct_before=count_text(light_log(),"direct detect ");
    const auto trigger_before=count_text(light_log(),"trigger detect ");
    const auto keys=[&](const std::string& key){cmd((prefix+key).c_str());};
    keys(init_key);keys("LSK1R");keys("LSK2L");wait(70);
    keys(fpln_key);keys("LSK6R");keys(sec_key);keys("LSK4L");wait(100);
    verify(count_text(light_log(),"direct detect ")==direct_before+2&&count_text(light_log(),"trigger detect ")==trigger_before,"With light retains INIT detection while suppressing F-PLN and SEC triggers");
  };
  check_light_keys("AirbusFBW/MCDU1","Init","Fpln","SecFpln");
  check_light_keys("AirbusFBW/MCDU2","Init","Fpln","SecFpln");
  cmd("AirbusFBW/MCDU2Data");cmd("AirbusFBW/MCDU2LSK4L");wait(60);
  verify(report().find("observed=1")!=std::string::npos&&light_log().find("direct detect MCDU2 INIT LSK1R success")!=std::string::npos&&light_log().find("direct detect MCDU2 INIT LSK2L success")!=std::string::npos,"INIT FROM/TO and ALTN publish data in With light without background capture");
  set("AirbusFBW/ECAMFlightPhase",0,6);

  set("sim/cockpit2/switches/beacon_on",0,1);wait(160);
  verify(light_log().find("light detect beacon try 1 success")!=std::string::npos,"beacon edge captures even outside full-open phase gating");
  const auto beacon_count=count_text(light_log(),"light detect beacon try 1\n");wait(100);
  verify(count_text(light_log(),"light detect beacon try 1\n")==beacon_count,"steady beacon does not retrigger");
  auto light_text=function<void(*)(const char*,const char*)>(mock,"MockSetData");
  light_text("AirbusFBW/MCDU2spw","BUSY");
  set("sim/cockpit2/switches/landing_lights_switch",0,1);wait(60);
  verify(light_log().find("light detect landing")==std::string::npos,"one landing light ON does not trigger");
  set("sim/cockpit2/switches/landing_lights_switch",1,1);wait(220);
  verify(light_log().find("light detect landing try 3 fail")!=std::string::npos&&light_log().find("light detect landing try 4")==std::string::npos,"landing edge permits exactly two retries when scratchpad blocks capture");
  light_text("AirbusFBW/MCDU2spw","");
  set("sim/cockpit2/switches/landing_lights_switch",1,0);wait(60);
  set("sim/cockpit2/switches/landing_lights_switch",1,1);wait(60);
  verify(light_log().find("light detect landing try 1 success")==std::string::npos,"one landing switch OFF then ON does not rearm");
  set("sim/cockpit2/switches/landing_lights_switch",0,0);
  set("sim/cockpit2/switches/landing_lights_switch",1,0);wait(60);
  set("sim/cockpit2/switches/landing_lights_switch",0,1);wait(60);
  set("sim/cockpit2/switches/landing_lights_switch",1,1);wait(160);
  verify(light_log().find("light detect landing try 1 success")!=std::string::npos,"new landing edge can capture after exhausted retries");
  verify(writes()==0&&std::string(fms_page())==before,"light capture restores page and never writes aircraft controls");
  set("AirbusFBW/ECAMFlightPhase",0,2);
  menu_click(menu("mcdu2_full_close"));advance(.1f);
  auto init=function<void(*)(int,const char*,const char*,const char*)>(mock,"MockFmsInit");
  for(int unit:{1,2}){
   scenario(std::string("native/direct-init/")+std::to_string(unit));
   cmd(("AirbusFBW/MCDU"+std::to_string(unit)+"Init").c_str());init(unit,"ZBAA","ZSSS","ZSPD");
   const auto page=std::string(fms_page());cmd(("AirbusFBW/MCDU"+std::to_string(unit)+"LSK1R").c_str());advance(.1f);
   wait(70);
   verify(report().find("destination=ZSSS")!=std::string::npos&&report().find("alternate=ZSPD ")!=std::string::npos,"INIT R1 alone also captures alternate");
   init(unit,"ZUCK","ZUUU","ZALT");
   cmd(("AirbusFBW/MCDU"+std::to_string(unit)+"LSK2L").c_str());advance(.1f);wait(70);
   const auto r=report();verify(r.find("origin=ZUCK departure_runway= destination=ZUUU arrival_runway=")!=std::string::npos&&r.find("alternate=ZALT ")!=std::string::npos&&std::string(fms_page())==page&&!menu_checked(menu("mcdu2_full_open")),"direct INIT reads operated side with automation OFF and without delayed page changes");
  }

  scenario("native/lr-backend-replacement");
  auto provider=function<void(*)(int)>(mock,"MockAircraftProvider");auto icao=function<void(*)(const char*)>(mock,"MockAircraftIcao");
  const auto lr_model=std::filesystem::u8path(argv[3])/"pruned-lr.acf";
  {std::ofstream f(lr_model);f<<"P acf/_studio Laminar Research\nP acf/_ICAO A333\nP acf/_cgZ 97\nP _gear/0/_gear_z 21.86\n";}
  menu_click(menu("mcdu2_full_open"));advance(.1f);
  function<void(*)()>(mock,"MockLrFms")();
  configure(argv[2],argv[3],lr_model.u8string().c_str());provider(0);icao("A333");message(0,106,nullptr);message(0,102,nullptr);wait(60);
  verify(report().find("AIRCRAFT_BACKEND id=lr")!=std::string::npos&&observers("AirbusFBW/")==0&&observers("sim/FMS")>0,"new backend removes previous aircraft key observers");

  set("laminar/A333/nd1/display_state",0,3);
  menu_click(menu("mcdu2_with_light"));wait(60);
  const auto lr_auto_before=count_text(light_log(),"auto detect try ");
  set("laminar/A333/data/flight_phase",0,2);wait(650);
  verify(count_text(light_log(),"auto detect try ")==lr_auto_before,"LR With light also disables phase automatic capture");
  for(int unit:{1,2}) {
    const auto direct_before=count_text(light_log(),"direct detect ");
    const auto trigger_before=count_text(light_log(),"trigger detect ");
    const auto prefix="sim/FMS"+std::to_string(unit)+"/";
    cmd((prefix+"index").c_str());
    const auto init_text=function<void(*)(const char*,const char*)>(mock,"MockSetData");
    const auto row_prefix="sim/cockpit2/radios/indicators/fms_cdu"+std::to_string(unit)+"_text_line";
    for(const auto& row:std::vector<std::pair<int,const char*>>{{0,"INIT"},{1,"CO RTE FROM/TO"},{2,"ZBAA/ZSSS"},{3,"ALTN/CO RTE"},{4,"ZSPD/----------"}})
      init_text((row_prefix+std::to_string(row.first)).c_str(),row.second);
    cmd((prefix+"ls_1r").c_str());cmd((prefix+"ls_2l").c_str());wait(70);
    for(const char* suffix:{"fpln","ls_6r","sec_fpln","ls_4l"})cmd((prefix+suffix).c_str());
    wait(100);
    verify(count_text(light_log(),"direct detect ")==direct_before+2&&count_text(light_log(),"trigger detect ")==trigger_before,"LR With light retains INIT detection while suppressing F-PLN and SEC triggers");
  }
  verify(report().find("destination=ZSSS")!=std::string::npos&&light_log().find("direct detect MCDU2 INIT LSK2L success")!=std::string::npos,"LR INIT data remains available in With light");
  cmd("sim/FMS2/data");cmd("sim/FMS2/ls_4l");wait(60);
  set("sim/cockpit2/switches/beacon_on",0,0);wait(60);
  const auto lr_beacon_before=count_text(light_log(),"light detect beacon try 1 success");
  set("sim/cockpit2/switches/beacon_on",0,1);wait(160);
  verify(count_text(light_log(),"light detect beacon try 1 success")==lr_beacon_before+1,"LR backend also executes light-triggered capture");

  scenario("native/zibo-deferred-direct-source");
  const auto zibo_model=std::filesystem::u8path(argv[3])/"pruned-zibo.acf";
  {std::ofstream f(zibo_model);f<<"P acf/_studio Laminar Research modified by Zibo and Twkster\nP acf/_ICAO B738\nP acf/_cgZ 59.5\nP _gear/0/_gear_z 13.59\n";}
  menu_click(menu("mcdu2_full_close"));advance(.1f);
  auto text=function<void(*)(const char*,const char*)>(mock,"MockSetData");
  text("laminar/B738/fms/ref_icao","ZUUU");text("laminar/B738/fms/ref_runway","02L");
  text("laminar/B738/fms/dest_icao","ZSPD");text("laminar/B738/fms/dest_runway","01L");
  configure(argv[2],argv[3],zibo_model.u8string().c_str());icao("B738");message(0,106,nullptr);message(0,102,nullptr);wait(60);
  verify(report().find("FMS source=ZIBO_DATAREF enabled=1")!=std::string::npos&&observers("sim/FMS")==0&&observers("AirbusFBW/")==0,"Zibo source works with MCDU automation disabled and no prior listeners");
  const int before_writes=writes();cmd("laminar/B738/button/fmc2_exec");advance(2.9f);
  verify(report().find("observed=0 age=")!=std::string::npos,"source does not read before key cooldown");advance(.2f);
  verify(report().find("destination=ZSPD")!=std::string::npos&&writes()==before_writes,"deferred read publishes endpoints without writes");

  scenario("native/unsupported-disable-stop-cleanup");
  const auto unsupported=std::filesystem::u8path(argv[3])/"pruned-unsupported.acf";{std::ofstream f(unsupported);f<<"P acf/_studio Other Vendor\nP acf/_ICAO A20N\n";}
  configure(argv[2],argv[3],unsupported.u8string().c_str());icao("A20N");provider(0);message(0,106,nullptr);message(0,102,nullptr);wait(60);
  verify(report().find("AIRCRAFT_BACKEND id=NONE")!=std::string::npos&&report().find("FMS source=NONE")!=std::string::npos&&observers("laminar/B738/button/fmc")==0,"unsupported vendor drops stale source and key observers");
  cmd("toliss_oans/route_plan");disable();verify(focus()==-1,"disable releases keyboard focus");cmd("toliss_oans/capt_airport");verify(focus()==-1,"disabled command cannot reacquire focus");
  stop();stop=nullptr;verify(resources()==0,"stop unregisters windows, commands, observers, worker callbacks");
  FreeLibrary(plugin);plugin=nullptr;FreeLibrary(mock);mock=nullptr;return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<" WindowsError="<<GetLastError()<<'\n';if(stop)stop();if(plugin)FreeLibrary(plugin);if(mock)FreeLibrary(mock);return 1;}
}
