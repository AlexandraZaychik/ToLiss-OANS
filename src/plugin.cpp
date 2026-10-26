// ToLiss OANS native plugin. See LICENSE and THIRD_PARTY_NOTICES.md.
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMDataAccess.h>
#include <XPLMDisplay.h>
#include <XPLMGraphics.h>
#include <XPLMMenus.h>
#include <XPLMPlanes.h>
#include <XPLMUtilities.h>
#include <windows.h>
#include <winhttp.h>
#include <GL/gl.h>
#include <GL/glu.h>
#include "core.hpp"
#include "detail_log.hpp"
#include "nd_ui.hpp"
#include "aircraft_profile.hpp"
#include "aircraft_profile_generator.hpp"
#include "gl_state.hpp"
#include "gl_coords.hpp"
#include "gl_font.hpp"
#include "a350_gl.hpp"
#include "map_texture.hpp"
#include "dsf_buildings.hpp"
#include "map_viewport.hpp"
#include "fms_xplm.hpp"
#include "runway_ahead.hpp"
#include <algorithm>
#include <cmath>
#include <condition_variable>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <limits>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>

using namespace oans;
namespace {
constexpr const char* version="0.4.34";
constexpr float white[3]={1,1,1},cyan[3]={.20f,.90f,1},green[3]={0,1,0},amber[3]={1,.65f,0};
constexpr float taxi_yellow[3]={.95f,.83f,0},magenta[3]={1,.35f,1};
AtlasFont map_font;
bool map_text=false;
std::filesystem::path plugin_dir,sim_dir,aircraft_dir;
void log(const std::string& s) {
    OANS_TRACE("EVENT "+s);
    XPLMDebugString(("[ToLissOANS] "+s+"\n").c_str());
    if(!plugin_dir.empty()) {std::ofstream f(plugin_dir/"diagnostics.log",std::ios::app);f<<s<<'\n';}
}
struct Ref {
    XPLMDataRef id=nullptr;int type=0,count=0;bool writable=false;
#if OANS_DEBUG
    std::string source;
#endif
    void bind(const char* name) {
        OANS_TRACE_SCOPE();OANS_TRACE(std::string("DataRef bind ")+name);
#if OANS_DEBUG
        source=name;
#endif
        id=XPLMFindDataRef(name);OANS_TRACE("XPLMFindDataRef returned; query type next");
        type=id?XPLMGetDataRefTypes(id):0;OANS_TRACE("XPLMGetDataRefTypes returned; query writable next");
        writable=id&&XPLMCanWriteDataRef(id);OANS_TRACE("XPLMCanWriteDataRef returned; query array count next");
        count=id?(type&xplmType_FloatArray?XPLMGetDatavf(id,nullptr,0,0):type&xplmType_IntArray?XPLMGetDatavi(id,nullptr,0,0):1):0;
        OANS_TRACE(std::string("DataRef bound ")+name+" found="+std::to_string(id!=nullptr)+" type="+std::to_string(type)+" count="+std::to_string(count)+" writable="+std::to_string(writable));
    }
    bool has(int i=0) const {return id&&i>=0&&i<count;}
    bool numeric(int i=0) const {return has(i)&&(type&(xplmType_Int|xplmType_Float|xplmType_Double|xplmType_IntArray|xplmType_FloatArray));}
    double get(int i=0,double fallback=0) const {
        OANS_TRACE_SCOPE();OANS_TRACE("DataRef read "+source+" index="+std::to_string(i)+" type="+std::to_string(type)+" count="+std::to_string(count));
        auto result=[&](double v){OANS_TRACE("DataRef result "+source+" index="+std::to_string(i)+" value="+std::to_string(v));return v;};
        if(!has(i))return result(fallback);
        if(type&xplmType_Double)return result(XPLMGetDatad(id));
        if(type&xplmType_Float)return result(XPLMGetDataf(id));
        if(type&xplmType_Int)return result(XPLMGetDatai(id));
        if(type&xplmType_FloatArray) {float v=0;return result(XPLMGetDatavf(id,&v,i,1)==1?v:fallback);}
        if(type&xplmType_IntArray) {int v=0;return result(XPLMGetDatavi(id,&v,i,1)==1?v:fallback);}
        return result(fallback);
    }
    bool array(std::vector<double>& out) const {
        OANS_TRACE_SCOPE();OANS_TRACE("DataRef array read "+source+" count="+std::to_string(count));
        out.clear();if(!id||count<=0)return false;
        if(type&xplmType_FloatArray) {
            std::vector<float> values(static_cast<size_t>(count));
            int n=XPLMGetDatavf(id,values.data(),0,count);if(n<=0)return false;
            out.assign(values.begin(),values.begin()+n);return true;
        }
        if(type&xplmType_IntArray) {
            std::vector<int> values(static_cast<size_t>(count));
            int n=XPLMGetDatavi(id,values.data(),0,count);if(n<=0)return false;
            out.assign(values.begin(),values.begin()+n);return true;
        }
        return false;
    }
};
std::map<std::string,Ref> refs;
Ref& dr(const std::string& n) {return refs.at(n);}
double value(const std::string& n,int i=0,double fallback=0) {return dr(n).get(i,fallback);}
const std::vector<std::pair<std::string,std::string>> names={
    {"lat","sim/flightmodel/position/latitude"},{"lon","sim/flightmodel/position/longitude"},
    {"speed","sim/flightmodel/position/groundspeed"},{"psi","sim/flightmodel/position/psi"},
    {"tcas_id","sim/cockpit2/tcas/targets/modeS_id"},
    {"tcas_lat","sim/cockpit2/tcas/targets/position/lat"},
    {"tcas_lon","sim/cockpit2/tcas/targets/position/lon"},
    {"tcas_psi","sim/cockpit2/tcas/targets/position/psi"},
    {"tcas_speed","sim/cockpit2/tcas/targets/position/V_msc"},
    {"tas","sim/flightmodel/position/true_airspeed"},{"magpsi","sim/flightmodel/position/mag_psi"},
    {"theta","sim/flightmodel/position/theta"},{"phi","sim/flightmodel/position/phi"},
    {"lx","sim/flightmodel/position/local_x"},{"ly","sim/flightmodel/position/local_y"},{"lz","sim/flightmodel/position/local_z"},
    {"ground","sim/flightmodel2/gear/on_ground"},{"paused","sim/time/paused"},
    {"range0","AirbusFBW/NDrangeCapt"},{"range1","AirbusFBW/NDrangeFO"},
    {"mode0","AirbusFBW/NDmodeCapt"},{"mode1","AirbusFBW/NDmodeFO"},
    {"map0","AirbusFBW/CaptMAPAvail"},{"map1","AirbusFBW/CoMAPAvail"},
    {"irs_align","AirbusFBW/TimeToAlign"},
    {"bright","AirbusFBW/DUBrightness"},{"bus","sim/cockpit2/electrical/bus_volts"},
    {"fallback","AirbusFBW/DisplayResolutionFallback"},
    {"swap0","AirbusFBW/PFDNDToggleCapt"},{"swap1","AirbusFBW/PFDNDToggleCo"},{"ecamSwap","AirbusFBW/ECAMNDSwitching"},
    {"view","sim/graphics/view/view_type"},{"vr","sim/graphics/VR/enabled"},
    {"world","sim/graphics/view/world_matrix"},{"acf","sim/graphics/view/acf_matrix"},{"projection","sim/graphics/view/projection_matrix_3d"},
    {"hx","sim/graphics/view/pilots_head_x"},{"hy","sim/graphics/view/pilots_head_y"},{"hz","sim/graphics/view/pilots_head_z"},
    {"hpsi","sim/graphics/view/pilots_head_psi"},{"htheta","sim/graphics/view/pilots_head_the"},{"hphi","sim/graphics/view/pilots_head_phi"},
    {"ww","sim/graphics/view/window_width"},{"wh","sim/graphics/view/window_height"},
    {"rt","sim/graphics/view/panel_render_type"},{"pl","sim/graphics/view/panel_total_pnl_l"},
    {"fbo","sim/graphics/view/current_gl_fbo"},{"reverseY","sim/graphics/view/is_reverse_y"},
    {"pb","sim/graphics/view/panel_total_pnl_b"},{"pr","sim/graphics/view/panel_total_pnl_r"},{"pt","sim/graphics/view/panel_total_pnl_t"},
    {"icao","sim/aircraft/view/acf_ICAO"},
};
struct Config {bool nd=true,test=false,fms=true,simple_stand_names=true,status_bar=false;};
std::string current_surface;
double surface_sample_at=0;
SurfaceTracker surface_tracker;
int status_bar_item=-1;
Config config;
double status_bar_height() {return config.status_bar&&!config.test?stus_bar_height:0;}
McduFmsPort fms_port;
RunwayAhead runway_advisory;
struct AdvisoryTest {
    std::string airport_query,runway_query,requested_airport,requested_runways;
    std::string message="Enter airport and runway, then Enable test.";
    bool active=false,pending=false,steady=true,replace=true,focused=false;
    int field=0;
    std::shared_ptr<Airport> airport;
    std::vector<RunwayAheadHit> hits;
} advisory_test;
double advisory_choose_at=0;
double fms_bind_at=0;
unsigned fms_generation_seen=0;
bool enabled=false,profile_ok=false,aircraft_ok=false,nd_on[2]={false,false},manual_airport=false;
bool airport_position_pending=true,first_oans_open=true;
double bind_at=0,model_check_at=0,choose_at=0,heartbeat=0,traffic_sample_at=0;
std::string status="INITIALIZING";
UiState views[3]; // captain, first officer, diagnostic popup
AirportDatabaseStatus airport_database_status;
Gesture gestures[3];
int input_owner=-1,captured_view=-1;
int cursor_view=-1,cursor_x=0,cursor_y=0;
double cursor_seen=-100;
std::optional<Vec2> pointer_on_map(int id);
bool search_built[3]={false,false,false};
int runway_index=-1;
XPLMWindowID window=nullptr,mouse_window=nullptr,test_window=nullptr;
XPLMMenuID menu=nullptr,settings_menu=nullptr,stand_name_menu=nullptr;int menu_item=-1;

bool draw_registered=false,loop_registered=false;
double render_gain=1;
std::vector<std::pair<XPLMCommandRef,int>> commands;
std::shared_ptr<Airport> airport;
std::vector<A350Traffic> traffic_targets;
bool aircraft_position_ready(int side=-1);
void refresh_tcas_targets() {
    OANS_TRACE_SCOPE();
    if(!aircraft_position_ready()) {traffic_targets.clear();return;}
    std::vector<double> ids,lats,lons,psis,speeds;
    if(!dr("tcas_id").array(ids)||!dr("tcas_lat").array(lats)||!dr("tcas_lon").array(lons)||
       !dr("tcas_psi").array(psis)||!dr("tcas_speed").array(speeds)) {traffic_targets.clear();return;}
    const size_t count=std::min({ids.size(),lats.size(),lons.size(),psis.size(),speeds.size()});
    std::vector<TcasTrafficSample> samples;samples.reserve(count);
    for(size_t i=0;i<count;++i)samples.push_back({static_cast<int>(i),static_cast<int>(ids[i]),
        lats[i],lons[i],psis[i],speeds[i]});
    traffic_targets=filter_tcas_traffic(samples,value("lat"),value("lon"));
}
std::vector<MapFeature> map_features[4];
void update_stand_names() {
    if(stand_name_menu) {
        XPLMCheckMenuItem(stand_name_menu,0,config.simple_stand_names?xplm_Menu_Checked:xplm_Menu_Unchecked);
        XPLMCheckMenuItem(stand_name_menu,1,config.simple_stand_names?xplm_Menu_Unchecked:xplm_Menu_Checked);
    }
    map_features[2].clear();
    if(airport)for(const auto& s:airport->stands)map_features[2].push_back({stand_name(s.name,config.simple_stand_names),s.pos});
    std::sort(map_features[2].begin(),map_features[2].end(),[](const auto& a,const auto& b){return a.name<b.name;});
    for(auto& view:views)if(view.feature_kind==2) {
        view.feature_selection=-1;view.feature_first=0;view.feature_initial='-';
        view.feature_dropdown=view.feature_initial_dropdown=false;
    }
}
std::optional<Homography> click_inverse[2];
std::array<Vec2,4> click_quad[2];
double click_update_at=-1;
double click_camera_gap=-1,click_window_size[2]={},click_screen_bounds[4]={};
std::array<double,16> click_world_used{},click_acf_used{},click_projection_used{};
int pass_count[3]={0,0,0},hit_count[2]={0,0};
struct PanelTrace {
    double time=-1,bounds[4]={},target[4]={};
    GLint viewport[4]={},polygon[2]={},blend_equation=0,scissor[4]={},program=0,vao=0,vbo=0;
    double projected[2][4]={};
    bool cull=false,logic=false,stencil=false,scissor_on=false;
    unsigned submitted[2]={0,0};
    std::string blocked[2];
};
PanelTrace panel_trace[3];
bool panel_probe_requested=false;
const char* nd_blocked(int side);
const char* nd_overlay_blocked(int side);
bool advisory_display_available(int side,bool pfd);
const Airport* displayed_advisory_airport();
std::vector<RunwayAheadHit> displayed_advisories() {
    if(advisory_test.pending)return {};
    const auto& hits=advisory_test.active?advisory_test.hits:runway_advisory.hits();
    return orient_runway_advisories(displayed_advisory_airport(),hits,
        dr("psi").numeric()?value("psi"):std::numeric_limits<double>::quiet_NaN());
}
bool advisory_blink();

// Background workers handle scenery parsing and OSM cache I/O; XPLM and OpenGL stay on the sim thread.
std::thread worker;
std::thread building_worker;
std::mutex db_mutex;
std::condition_variable db_cv;
std::condition_variable building_cv;
std::atomic<bool> stop_worker{false},index_ready{false};
std::atomic<bool> airport_map_loading{true};
std::vector<IndexEntry> airport_index;
std::vector<IndexEntry> unfiltered_airport_index;
double airport_minimum_runway=0; // Protected by db_mutex, including worker publication.
double airport_minimum_width=0;
std::optional<IndexEntry> request;
std::optional<IndexEntry> advisory_request;
std::shared_ptr<Airport> advisory_airport,pending_advisory;
std::string advisory_requested;
std::shared_ptr<Airport> pending_airport;
std::shared_ptr<A350Map> a350_map,pending_map;
std::array<MapTexture,3> map_textures;
std::array<std::weak_ptr<A350Map>,3> texture_maps;
std::string pending_map_note,pending_map_icao;
bool pending_map_is_building_refresh=false;
std::deque<IndexEntry> building_queue;
std::set<std::string> building_pending;
std::unique_ptr<A350Map> a350_ui;
Box oans_box;
std::string worker_error,last_requested;
bool aircraft_position_ready(int side) {
    // Simulator coordinates can exist before the aircraft has an IRS position.
    // MAP AVAIL can already be true while IRS alignment is still counting down.
    // Missing/invalid alignment status must not reveal simulator-truth positions.
    return aircraft_ok&&dr("lat").numeric()&&dr("lon").numeric()&&
        dr("irs_align").numeric()&&std::isfinite(value("irs_align"))&&value("irs_align")<=0&&
        std::isfinite(value("lat"))&&std::abs(value("lat"))<=90&&
        std::isfinite(value("lon"))&&std::abs(value("lon"))<=180&&
        dr("bus").numeric(1)&&std::max(value("bus",0),value("bus",1))>=20&&
        ((side!=1&&dr("map0").numeric()&&value("map0")>=.5)||
         (side!=0&&dr("map1").numeric()&&value("map1")>=.5));
}
const Airport* displayed_advisory_airport() {return advisory_test.active?advisory_test.airport.get():advisory_airport.get();}
bool advisory_blink() {
    if(advisory_test.active)return advisory_test.steady||static_cast<int>(std::floor(heartbeat*2))%2==0;
    return runway_advisory.blink();
}

bool has_building_geometry(const A350Map& map) {
    const auto stats=map.stats();
    return stats.terminals>0||stats.buildings>0;
}

struct WinHttpHandle {
    HINTERNET value=nullptr;
    ~WinHttpHandle(){if(value)WinHttpCloseHandle(value);}
};

std::string winhttp_error(const char* operation) {
    return std::string(operation)+" failed (WinHTTP "+std::to_string(GetLastError())+")";
}

std::string form_encode(const std::string& input) {
    static constexpr char hex[]="0123456789ABCDEF";
    std::string output;output.reserve(input.size()*3);
    for(unsigned char c:input) {
        if((c>='A'&&c<='Z')||(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'||c=='~')output.push_back(static_cast<char>(c));
        else if(c==' ')output.push_back('+');
        else {output.push_back('%');output.push_back(hex[c>>4]);output.push_back(hex[c&15]);}
    }
    return output;
}

std::string post_overpass(const wchar_t* host,const std::string& body) {
    WinHttpHandle session{WinHttpOpen(L"ToLissOANS/0.4.34 airport-map",WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                      WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value)throw std::runtime_error(winhttp_error("WinHttpOpen"));
    if(!WinHttpSetTimeouts(session.value,10000,10000,20000,45000))
        throw std::runtime_error(winhttp_error("WinHttpSetTimeouts"));
    WinHttpHandle connection{WinHttpConnect(session.value,host,INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection.value)throw std::runtime_error(winhttp_error("WinHttpConnect"));
    WinHttpHandle request_handle{WinHttpOpenRequest(connection.value,L"POST",L"/api/interpreter",nullptr,
                                                    WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request_handle.value)throw std::runtime_error(winhttp_error("WinHttpOpenRequest"));
    constexpr wchar_t headers[]=L"Content-Type: application/x-www-form-urlencoded\r\n";
    if(!WinHttpSendRequest(request_handle.value,headers,static_cast<DWORD>(-1),
                           const_cast<char*>(body.data()),static_cast<DWORD>(body.size()),
                           static_cast<DWORD>(body.size()),0))
        throw std::runtime_error(winhttp_error("WinHttpSendRequest"));
    if(!WinHttpReceiveResponse(request_handle.value,nullptr))
        throw std::runtime_error(winhttp_error("WinHttpReceiveResponse"));
    DWORD status_code=0,status_size=sizeof(status_code);
    if(!WinHttpQueryHeaders(request_handle.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX,&status_code,&status_size,WINHTTP_NO_HEADER_INDEX))
        throw std::runtime_error(winhttp_error("WinHttpQueryHeaders"));
    if(status_code<200||status_code>=300)throw std::runtime_error("Overpass HTTP status "+std::to_string(status_code));
    std::string response;constexpr size_t max_response=32u*1024u*1024u;
    for(;;) {
        DWORD available=0;
        if(!WinHttpQueryDataAvailable(request_handle.value,&available))
            throw std::runtime_error(winhttp_error("WinHttpQueryDataAvailable"));
        if(!available)break;
        if(response.size()+available>max_response)throw std::runtime_error("Overpass response exceeds 32 MiB");
        const size_t offset=response.size();response.resize(offset+available);
        DWORD received=0;
        if(!WinHttpReadData(request_handle.value,response.data()+offset,available,&received))
            throw std::runtime_error(winhttp_error("WinHttpReadData"));
        response.resize(offset+received);
        if(!received)break;
    }
    return response;
}

bool has_nonempty_osm_elements(const std::string& json) {
    const auto key=json.find("\"elements\"");if(key==std::string::npos)return false;
    auto at=json.find(':',key+9);if(at==std::string::npos)return false;
    ++at;while(at<json.size()&&std::isspace(static_cast<unsigned char>(json[at])))++at;
    if(at>=json.size()||json[at++]!='[')return false;
    while(at<json.size()&&std::isspace(static_cast<unsigned char>(json[at])))++at;
    return at<json.size()&&json[at]!=']';
}

std::shared_ptr<A350Map> download_osm_map(const IndexEntry& entry,const std::filesystem::path& assets,
                                          const std::filesystem::path& cache_file,const std::atomic<bool>& cancel,
                                          std::string& warning) {
    OANS_TRACE_SCOPE();
    std::string icao=entry.icao;
    std::transform(icao.begin(),icao.end(),icao.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
    if(icao.size()<3||icao.size()>8||!std::all_of(icao.begin(),icao.end(),[](unsigned char c){return (c>='A'&&c<='Z')||(c>='0'&&c<='9');})) {
        warning="invalid airport code for OSM query";return {};
    }
    const std::string query="[out:json][timeout:120];area[\"aeroway\"=\"aerodrome\"][\"icao\"=\""+icao+"\"]->.a;"
        "(way[\"building\"](area.a);way[\"aeroway\"=\"terminal\"](area.a);)->.w;"
        "(rel[\"building\"](area.a);rel[\"aeroway\"=\"terminal\"](area.a);)->.r;"
        ".w out tags geom;.r out body geom;";
    const std::string body="data="+form_encode(query);
    const auto temporary=std::filesystem::path(cache_file.wstring()+L".download");
    std::error_code ignored;
    std::filesystem::create_directories(cache_file.parent_path(),ignored);
    if(ignored) {warning="cannot create OSM cache directory: "+ignored.message();return {};}
    std::string errors;
    constexpr std::array<const wchar_t*,2> endpoints={L"overpass-api.de",L"overpass.private.coffee"};
    for(const wchar_t* host:endpoints) {
        if(cancel.load())return {};
        try {
            const auto response=post_overpass(host,body);
            if(!has_nonempty_osm_elements(response))throw std::runtime_error("Overpass returned no building elements");
            {
                std::ofstream file(temporary,std::ios::binary|std::ios::trunc);
                if(!file)throw std::runtime_error("cannot create temporary OSM cache");
                file.write(response.data(),static_cast<std::streamsize>(response.size()));
                if(!file)throw std::runtime_error("cannot write temporary OSM cache");
            }
            auto map=std::make_shared<A350Map>(assets,entry,temporary,&cancel);
            if(cancel.load()) {std::filesystem::remove(temporary,ignored);return {};}
            if(!MoveFileExW(temporary.c_str(),cache_file.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))
                throw std::runtime_error("cannot commit OSM cache (Windows "+std::to_string(GetLastError())+")");
            return map;
        } catch(const std::exception& e) {
            std::filesystem::remove(temporary,ignored);
            if(!errors.empty())errors+="; ";
            errors+=e.what();
        }
    }
    warning="OSM fallback failed: "+errors;return {};
}

struct MapLoadResult {std::shared_ptr<A350Map> map;std::string note;bool download_osm=false;};

MapLoadResult load_airport_map(const IndexEntry& entry,const std::filesystem::path& plugin_root,
                               const std::atomic<bool>& cancel) {
    OANS_TRACE_SCOPE();
    const auto assets=plugin_root/"assets";
    const auto dsf_file=plugin_root/"cache"/("dsf_buildings_"+entry.icao+".json");
    const auto osm_file=plugin_root/"cache"/("osm2_"+entry.icao+".json");
    std::shared_ptr<A350Map> dsf_map;
    std::string dsf_reason;
    auto is_regular_file=[](const std::filesystem::path& path) {
        std::error_code error;return std::filesystem::is_regular_file(path,error);
    };
    if(cancel.load())return {};
    if(is_regular_file(dsf_file)) {
        try {
            auto map=std::make_shared<A350Map>(assets,entry,dsf_file,&cancel);
            if(cancel.load())return {};
            if(has_building_geometry(*map))return {std::move(map),"Building source: reused DSF cache for "+entry.icao,false};
        } catch(const std::exception&) {
            // Missing, unreadable, corrupt or empty caches are rebuilt from scenery.
        }
    }
    try {
        OANS_TRACE("DSF extraction begin airport="+entry.icao);
        const auto extracted=extract_runtime_dsf(sim_dir,entry,plugin_root/"cache",cancel);
        dsf_reason=extracted.note;
        if(!extracted.cache.empty()&&!cancel.load()) {
            dsf_map=std::make_shared<A350Map>(assets,entry,extracted.cache,&cancel);
            if(has_building_geometry(*dsf_map))return {std::move(dsf_map),extracted.note,false};
            dsf_reason="runtime DSF footprints could not produce map geometry";
        }
    } catch(const std::exception& e) {dsf_reason=std::string("runtime DSF extraction failed: ")+e.what();}
    if(cancel.load())return {};
    if(is_regular_file(osm_file)) {
        try {
            auto map=std::make_shared<A350Map>(assets,entry,osm_file,&cancel);
            if(has_building_geometry(*map))return {std::move(map),"Building fallback: reused cached OSM data ("+dsf_reason+")",false};
        } catch(const std::exception&) {
            // A damaged cache should not block a fresh request; the old file is replaced only after validation.
        }
    }
    if(cancel.load())return {};
    if(!dsf_map)dsf_map=std::make_shared<A350Map>(assets,entry,std::filesystem::path{},&cancel);
    return {std::move(dsf_map),"Building fallback: OSM download queued in background ("+dsf_reason+")",true};
}

void building_worker_main() {
    OANS_TRACE_SCOPE();
    while(!stop_worker) {
        std::optional<IndexEntry> job;
        {
            std::unique_lock<std::mutex> lock(db_mutex);
            building_cv.wait(lock,[]{return stop_worker||!building_queue.empty();});
            if(stop_worker)return;
            job=std::move(building_queue.front());building_queue.pop_front();
        }
        std::string warning;
        std::shared_ptr<A350Map> map;
        std::string note;
        try {
            OANS_TRACE("Building job "+job->icao);
            auto loaded=load_airport_map(*job,plugin_dir,stop_worker);
            map=std::move(loaded.map);note=std::move(loaded.note);
            if(loaded.download_osm&&!stop_worker) {
                auto downloaded=download_osm_map(*job,plugin_dir/"assets",
                    plugin_dir/"cache"/("osm2_"+job->icao+".json"),stop_worker,warning);
                if(downloaded) {map=std::move(downloaded);note="Building fallback: downloaded and cached OSM data for "+job->icao+"; "+note;}
            }
        } catch(const std::exception& e) {warning=e.what();}
        std::lock_guard<std::mutex> lock(db_mutex);
        building_pending.erase(job->icao);
        if(stop_worker)continue;
        if(map&&last_requested==job->icao&&(!pending_airport||pending_airport->icao==job->icao)) {
            pending_map=std::move(map);pending_map_icao=job->icao;pending_map_is_building_refresh=true;
            pending_map_note=std::move(note);
            if(!warning.empty())pending_map_note+="; "+warning;
        } else if(!warning.empty()&&last_requested==job->icao) {
            pending_map_note="Building fallback failed for "+job->icao+": "+warning;pending_map_icao=job->icao;
        }
    }
}

void worker_main() {
    OANS_TRACE_SCOPE();
    try {
        std::vector<IndexEntry> result;
        for(const auto& file:scenery_files(sim_dir)) {
            if(stop_worker)return;
            OANS_TRACE("Index scanning "+file.u8string());
            auto entries=scan_index(file,&stop_worker);result.insert(result.end(),entries.begin(),entries.end());
        }
        {std::lock_guard<std::mutex> lock(db_mutex);
            unfiltered_airport_index=canonical_index(result);
            airport_index=filter_airport_index(unfiltered_airport_index,airport_minimum_runway,airport_minimum_width);}
        index_ready=true;
        while(!stop_worker) {
            std::optional<IndexEntry> job;bool advisory_job=false;
            {std::unique_lock<std::mutex> lock(db_mutex);db_cv.wait(lock,[]{return stop_worker||request.has_value()||advisory_request.has_value();});
                if(stop_worker)return;
                // Ownship advisory data takes priority over optional map geometry.
                if(advisory_request) {job=std::move(advisory_request);advisory_request.reset();advisory_job=true;}
                else {job=std::move(request);request.reset();}}
            try {
                OANS_TRACE("Worker airport="+job->icao+" file="+job->file.u8string()+" offset="+std::to_string(job->offset));
                auto loaded=std::make_shared<Airport>(load_airport(*job));
                if(advisory_job) {std::lock_guard<std::mutex> lock(db_mutex);pending_advisory=std::move(loaded);continue;}
                // Publish airport/runway geometry immediately. Scenery decoding and
                // network fallback use the separate building worker, so advisory
                // requests never wait behind DSF IO/decompression.
                MapLoadResult map_result{std::make_shared<A350Map>(plugin_dir/"assets",*job,std::filesystem::path{},&stop_worker),
                    "Buildings: checking DSF cache and local scenery in background",true};
                if(!map_result.map)continue;
                std::lock_guard<std::mutex> lock(db_mutex);
                if(last_requested!=job->icao)continue;
                pending_airport=std::move(loaded);
                if(!(pending_map_is_building_refresh&&pending_map_icao==job->icao)) {
                    pending_map=std::move(map_result.map);pending_map_note=std::move(map_result.note);
                    pending_map_icao=job->icao;pending_map_is_building_refresh=false;
                }
                if(map_result.download_osm&&!stop_worker&&last_requested==job->icao&&
                   building_pending.insert(job->icao).second) {
                    building_queue.push_back(*job);building_cv.notify_one();
                }
            }
            catch(const std::exception& e) {std::lock_guard<std::mutex> lock(db_mutex);
                if(advisory_job?advisory_requested==job->icao:last_requested==job->icao) {
                    worker_error=e.what();if(!advisory_job)airport_map_loading=false;
                }}
        }
    } catch(const std::exception& e) {std::lock_guard<std::mutex> lock(db_mutex);worker_error=e.what();airport_map_loading=false;index_ready=true;}
}
void start_worker() {
    OANS_TRACE_SCOPE();
    stop_worker=false;
    {std::lock_guard<std::mutex> lock(db_mutex);building_queue.clear();building_pending.clear();pending_map_is_building_refresh=false;}
    building_worker=std::thread(building_worker_main);worker=std::thread(worker_main);
}
void finish_worker() {
    OANS_TRACE_SCOPE();
    stop_worker=true;db_cv.notify_all();building_cv.notify_all();
    if(worker.joinable())worker.join();
    if(building_worker.joinable())building_worker.join();
}
void request_airport(const IndexEntry& e) {
    OANS_TRACE_SCOPE();
    std::lock_guard<std::mutex> lock(db_mutex);airport_map_loading=true;request=e;last_requested=e.icao;db_cv.notify_one();
}
void read_config() {
    OANS_TRACE_SCOPE();
    config={};std::ifstream in(plugin_dir/"config.ini");std::string line;
    OANS_CONFIG("config.ini path="+(plugin_dir/"config.ini").u8string()+(in?" found; reading":" missing/unreadable; using defaults"));
    while(std::getline(in,line)) {
        const auto p=line.find('=');if(p==std::string::npos)continue;
        std::string key=line.substr(0,p),v=line.substr(p+1);
        key.erase(std::remove_if(key.begin(),key.end(),[](unsigned char c){return std::isspace(c);}),key.end());
        bool yes=std::atoi(v.c_str())==1;
        if(key=="stand_name")config.simple_stand_names=v.find("complex")==std::string::npos;
        if(key=="stus_bar")config.status_bar=yes;
        if(key=="nd_enabled")config.nd=yes;
        if(key=="test_pattern")config.test=yes;
        if(key=="fms_mcdu2_sampling")config.fms=yes;
    }
    OANS_CONFIG("config.ini resolved nd="+std::to_string(config.nd)+" test="+std::to_string(config.test)+" fms="+std::to_string(config.fms)+" simple_stands="+std::to_string(config.simple_stand_names));
    if(in.bad())OANS_CONFIG("config.ini read error; settings may be partial");
    update_stand_names();
    if(settings_menu&&status_bar_item>=0)XPLMCheckMenuItem(settings_menu,status_bar_item,config.status_bar?xplm_Menu_Checked:xplm_Menu_Unchecked);
    current_surface.clear();
    surface_sample_at=0;
    surface_tracker.reset();
}
void bind_refs(bool aircraft_refs=true) {
    OANS_TRACE_SCOPE();
    for(const auto& n:names) {
        // Aircraft providers can load AFTER us. In XPluginStart, resolve only
        // simulator-owned interfaces; first flight loop resolves providers.
        if(!aircraft_refs&&n.second.rfind("AirbusFBW/",0)==0)refs[n.first]=Ref{};
        else refs[n.first].bind(n.second.c_str());
    }
}
void diagnostics() {
    OANS_TRACE_SCOPE();
    if(config.test)panel_probe_requested=true;
    std::ofstream f(plugin_dir/"interfaces.txt");
    f<<"ToLissOANS "<<version<<" | runtime interface snapshot\nprofile_match="<<profile_ok<<" aircraft_match="<<aircraft_ok
     <<"\n";
    f<<"OANS_POSITION ready="<<aircraft_position_ready()<<" pending="<<airport_position_pending
     <<" first_open="<<first_oans_open<<" manual="<<manual_airport
     <<" airport="<<(airport?airport->icao:"NONE")<<" requested="<<last_requested<<'\n';
    f<<"OANS_STUS_BAR enabled="<<config.status_bar<<" surface="<<current_surface<<'\n';
    f<<"OANS_EMPTY_MAP message="<<(airport?"NONE":airport_map_loading?"PLEASE WAIT":"ARPT NAV NOT AVAIL")<<'\n';
    f<<"OANS_IRS status_present="<<dr("irs_align").numeric()<<" seconds_remaining="<<value("irs_align")<<'\n';
    const auto* alert_airport=displayed_advisory_airport();
    f<<"RWY_AHEAD airport="<<(alert_airport?alert_airport->icao:"NONE")
     <<" low_speed_seconds="<<runway_advisory.low_speed_seconds()<<" count="<<displayed_advisories().size()
     <<" blink="<<advisory_blink()<<" source="<<(advisory_test.active?"TEST":"LIVE")<<'\n';
    for(const auto& hit:displayed_advisories())f<<"RWY_AHEAD runway="<<hit.first<<"-"<<hit.second<<" eta="<<hit.seconds<<" distance="<<hit.distance<<'\n';
    f<<"TCAS traffic_icons="<<traffic_targets.size()<<" refresh_at="<<traffic_sample_at<<'\n';
    f<<"OANS_DATABASE cycle="<<airport_database_status.cycle<<" validity="<<airport_database_status.validity
     <<" source=Custom Data/cycle_info.txt\n";
    const auto& plan=fms_port.sampler.plan();
    f<<"FMS source=MCDU2 enabled="<<config.fms<<" bound="<<fms_port.ready()<<" busy="<<fms_port.sampler.busy()
     <<" observed="<<plan.observed<<" age="<<fms_port.sampler.age(XPLMGetElapsedTime())<<" status="<<fms_port.sampler.status()
     <<" plan_status="<<plan.reason<<" origin="<<plan.origin.airport<<" departure_runway="<<plan.origin.runway
     <<" destination="<<plan.destination.airport<<" arrival_runway="<<plan.destination.runway<<" alternate="<<plan.alternate
     <<" saved_page="<<mcdu_page_name(fms_port.sampler.saved_page())<<'\n';
    f<<"RWY_AHEAD_TEST active="<<advisory_test.active<<" pending="<<advisory_test.pending<<" steady="<<advisory_test.steady
     <<" airport="<<advisory_test.airport_query<<" runway="<<advisory_test.runway_query<<" message="<<advisory_test.message<<'\n';
    for(int side=0;side<2;++side)f<<"RWY_AHEAD_display side="<<side<<" nd="<<advisory_display_available(side,false)
        <<" pfd="<<advisory_display_available(side,true)<<'\n';
    if(a350_map)for(int side=0;side<3;++side){const auto c=a350_map->cache_stats(side);
        f<<"A350_cache side="<<side<<" builds="<<c.builds<<" hits="<<c.hits<<" last_ms="<<c.last_ms
         <<" mean_ms="<<(c.builds?c.total_ms/c.builds:0)<<'\n';
        for(const auto& label:a350_map->runway_labels(side))
            f<<"runway_label side="<<side<<" pair="<<label.first<<'-'<<label.second<<" selected="<<label.selected
             <<" center="<<label.center.x<<','<<label.center.y<<" visible_start="<<label.start.x<<','<<label.start.y
             <<" visible_end="<<label.end.x<<','<<label.end.y<<'\n';}
    for(const auto& n:names) {const auto& r=dr(n.first);f<<n.second<<" present="<<(r.id!=nullptr)<<" type="<<r.type<<" count="<<r.count<<" writable="<<r.writable<<" value="<<r.get()<<'\n';}
    f<<"panel_passes="<<pass_count[0]<<','<<pass_count[1]<<','<<pass_count[2]<<"\nND_clicks="<<hit_count[0]<<','<<hit_count[1]<<'\n';
    f<<"click_projection time="<<click_update_at;
    for(int side=0;side<2;++side) {f<<" side="<<side<<" valid="<<bool(click_inverse[side]);for(auto q:click_quad[side])f<<" xy="<<q.x<<','<<q.y;}
    std::optional<Vec2> mapped_cursor;if(cursor_view>=0)mapped_cursor=pointer_on_map(cursor_view);
    f<<'\n'<<std::setprecision(9)<<"click_context d="<<click_camera_gap<<" source=acf_matrix"
     <<" cursor="<<cursor_view<<" raw_xy="<<cursor_x<<','<<cursor_y
     <<" cursor_age="<<XPLMGetElapsedTime()-cursor_seen
     <<" window="<<click_window_size[0]<<','<<click_window_size[1]
     <<" screen="<<click_screen_bounds[0]<<','<<click_screen_bounds[1]<<','<<click_screen_bounds[2]<<','<<click_screen_bounds[3]
     <<" mapped="<<bool(mapped_cursor);
    if(mapped_cursor)f<<' '<<mapped_cursor->x<<','<<mapped_cursor->y;
    f<<"\nclick_world_used";for(double v:click_world_used)f<<' '<<v;
    f<<"\nclick_acf_used";for(double v:click_acf_used)f<<' '<<v;
    f<<"\nclick_projection_used";for(double v:click_projection_used)f<<' '<<v;
    f<<std::setprecision(6)<<'\n';
    for(int i=0;i<3;++i) {
        const auto& p=panel_trace[i];
        f<<"gauge_capture pass="<<i<<" time="<<p.time<<" bounds="<<p.bounds[0]<<','<<p.bounds[1]<<','<<p.bounds[2]<<','<<p.bounds[3]
         <<" viewport="<<p.viewport[0]<<','<<p.viewport[1]<<','<<p.viewport[2]<<','<<p.viewport[3]
         <<" draw_target="<<p.target[0]<<','<<p.target[1]<<','<<p.target[2]<<','<<p.target[3]
         <<" cull="<<p.cull<<" logic="<<p.logic<<" stencil="<<p.stencil
         <<" polygon="<<p.polygon[0]<<','<<p.polygon[1]<<" blend_equation="<<p.blend_equation
         <<" program="<<p.program<<" vao="<<p.vao<<" vbo="<<p.vbo
         <<" scissor_on="<<p.scissor_on<<" scissor="<<p.scissor[0]<<','<<p.scissor[1]<<','<<p.scissor[2]<<','<<p.scissor[3]
         <<" submitted="<<p.submitted[0]<<','<<p.submitted[1]<<" blocked="<<p.blocked[0]<<'|'<<p.blocked[1]<<'\n';
        for(int side=0;side<2;++side)f<<"gauge_projected pass="<<i<<" side="<<side<<" rect="<<p.projected[side][0]<<','<<p.projected[side][1]<<','<<p.projected[side][2]<<','<<p.projected[side][3]<<'\n';
    }
    for(int side=0;side<2;++side) {
        f<<"ND_state side="<<side<<" enabled="<<nd_on[side]<<" blocked="<<nd_blocked(side)<<'\n';
        f<<"OANS_switch side="<<side<<" visible="<<(nd_overlay_blocked(side)[0]=='\0')<<" checked="<<nd_on[side]
         <<" bounds="<<oans_box.l<<','<<oans_box.b<<','<<oans_box.r<<','<<oans_box.t<<'\n';
    }
    for(int i=0;i<3;++i)f<<"view="<<i<<" panel="<<static_cast<int>(views[i].panel)<<" range_nm="<<views[i].range_nm
        <<" follow="<<views[i].follow<<" query="<<views[i].query<<" search_results="<<views[i].results.size()
        <<" airport_mode="<<views[i].airport_mode<<" initial="<<views[i].initial
        <<" candidate="<<(views[i].selection>=0&&views[i].selection<int(views[i].results.size())?views[i].results[views[i].selection].icao:"NONE")
        <<" menu="<<views[i].menu<<" editing="<<views[i].editing<<" crosses="<<views[i].crosses.size()<<" flags="<<views[i].flags.size()
        <<" center="<<views[i].center.x<<','<<views[i].center.y<<" cursor="<<(cursor_view==i)
        <<" airport_first="<<views[i].airport_first<<" airport_dropdown="<<views[i].airport_dropdown<<" initial_first="<<views[i].initial_first
        <<" feature_kind="<<views[i].feature_kind<<" feature_selection="<<views[i].feature_selection<<" feature_first="<<views[i].feature_first
        <<" feature_dropdown="<<views[i].feature_dropdown<<" feature_initial_dropdown="<<views[i].feature_initial_dropdown
        <<" confirmed_airport="<<(views[i].confirmed_airport?views[i].confirmed_airport->icao:"NONE")<<" airport_modifying="<<views[i].airport_modifying
        <<" not_in_plan="<<(airport&&airport_outside_active_plan(fms_port.sampler.plan(),airport->icao))<<'\n';
    log("Interface report saved: "+(plugin_dir/"interfaces.txt").u8string());
}
void nearest_airport();
void inspect_model() {
    OANS_TRACE_SCOPE();
    char name[512]={},path[2048]={};XPLMGetNthAircraftModel(0,name,path);
    aircraft_dir=std::filesystem::u8path(path).parent_path();
    char icao[32]={};if(dr("icao").id)XPLMGetDatab(dr("icao").id,icao,0,31);
    std::string profile_error;
    const std::string aircraft_icao(icao);
    OANS_CONFIG("Aircraft model="+std::string(name)+" path="+path+" ICAO="+aircraft_icao);
    const double minimum=minimum_runway_length(aircraft_icao);
    const double width=minimum_runway_width(aircraft_icao);
    bool filter_changed=false;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        if(minimum!=airport_minimum_runway||width!=airport_minimum_width) {
            airport_minimum_runway=minimum;
            airport_minimum_width=width;
            airport_index=filter_airport_index(unfiltered_airport_index,minimum,width);
            filter_changed=true;
        }
    }
    if(filter_changed) {
        log("Airport database requires one runway at least "+std::to_string(static_cast<int>(minimum))+" m long and "+std::to_string(static_cast<int>(width))+" m wide ("+aircraft_icao+")");
        if(index_ready) {
            nearest_airport();runway_advisory.reset();advisory_airport.reset();advisory_choose_at=0;
            std::lock_guard<std::mutex> lock(db_mutex);
            advisory_request.reset();pending_advisory.reset();advisory_requested.clear();
        }
    }
    std::filesystem::path loaded_profile;
    profile_ok=profile::load_for_aircraft(plugin_dir/"config",std::filesystem::u8path(path),aircraft_icao,loaded_profile,profile_error);
    if(profile_ok)log("Loaded aircraft profile: "+loaded_profile.u8string());
    aircraft_ok=profile_ok&&dr("range0").numeric()&&dr("mode0").numeric();
    if(!profile_ok)log(profile_error);
    OANS_CONFIG("Profile result valid="+std::to_string(profile_ok)+" path="+loaded_profile.u8string()+" aircraft_interfaces_ready="+std::to_string(aircraft_ok)+" error="+profile_error);
    status=profile_ok?"AIRCRAFT PROFILE LOADED: "+std::string(icao):"ND BLOCKED: AIRCRAFT PROFILE MISSING OR INVALID";
    log(status+" ("+name+")");
}
void select_runway(int i) {
    OANS_TRACE_SCOPE();
    if(!airport||airport->runways.empty())return;
    runway_index=(i+static_cast<int>(airport->runways.size()))%airport->runways.size();
    status="RWY "+airport->runways[runway_index].name+" SELECTED";log(status);
}
void release_input() {
    input_owner=-1;captured_view=-1;
    for(int i=0;i<3;++i) {views[i].editing=false;gestures[i].clear();}
    XPLMTakeKeyboardFocus(nullptr);
}
void focus_airport(int id) {
    if(input_owner>=0&&input_owner!=id)release_input();
    XPLMTakeKeyboardFocus(id==2?window:mouse_window);
    input_owner=id;for(int i=0;i<3;++i)views[i].editing=i==id;
}
void refresh_search(int id) {
    std::lock_guard<std::mutex> lock(db_mutex);
    search_airports(views[id],airport_index,value("lat"),value("lon"));
    search_built[id]=index_ready;
}
void open_panel(int id,Panel panel) {
    if(input_owner>=0)release_input();
    // Panel commands must not bypass the crew's OANS checkbox.
    if(id<2&&panel!=Panel::closed&&nd_blocked(id)[0]) {status=nd_blocked(id);return;}
    auto& s=views[id];s.panel=panel;s.menu=false;
    s.initial_dropdown=s.airport_dropdown=false;
    s.feature_dropdown=s.feature_initial_dropdown=false;
    if(panel!=Panel::closed)s.last_panel=panel;
    if(panel==Panel::airport) {
        if(!s.confirmed_airport&&!s.airport_modifying&&s.query.empty()) {
            std::lock_guard<std::mutex> lock(db_mutex);
            if(airport) {
                s.confirmed_airport=find_airport(airport_index,airport->icao);
                select_airport_candidate(s,airport_index,airport->icao);
            }
        }
        refresh_search(id);focus_airport(id);
    }
    log("ND UI view="+std::to_string(id)+" panel="+std::to_string(static_cast<int>(panel)));
}
void nearest_airport() {
    release_input();status="NEAREST AIRPORT MODE";manual_airport=false;airport.reset();a350_map.reset();
    runway_index=-1;choose_at=0;airport_position_pending=true;
    {
        std::lock_guard<std::mutex> lock(db_mutex);
        airport_map_loading=true;request.reset();last_requested.clear();pending_airport.reset();pending_map.reset();
        pending_map_note.clear();pending_map_icao.clear();pending_map_is_building_refresh=false;worker_error.clear();
    }
    for(auto& items:map_features)items.clear();
    for(int i=0;i<3;++i) {
        auto& s=views[i];s.follow=true;s.center={};s.crosses.clear();s.flags.clear();
        s.feature_selection=-1;s.feature_dropdown=s.feature_initial_dropdown=false;s.feature_first=0;s.feature_initial='-';
        s.results.clear();s.selection=-1;s.query.clear();s.confirmed_airport.reset();s.airport_modifying=false;search_built[i]=false;
    }
    status="WAITING FOR AIRCRAFT POSITION";
}
void confirm_first_oans_position() {
    if(!first_oans_open)return;
    first_oans_open=false;
    // An explicit selection in this flight still belongs to the crew.
    if(!manual_airport&&!advisory_test.active&&!advisory_test.pending)nearest_airport();
}
void toggle_nd(int side) {
    nd_on[side]=!nd_on[side];
    if(nd_on[side])confirm_first_oans_position();
}
void display_airport(int id) {
    auto& s=views[id];
    if(s.selection<0||s.selection>=static_cast<int>(s.results.size())) {status="SELECT AIRPORT FROM RESULTS";return;}
    // The result carries the canonical 1302 identity and its original source offset.
    const auto entry=s.results[s.selection];confirm_airport_candidate(s);release_input();status="AIRPORT CHANGED";
    manual_airport=true;airport_position_pending=false;request_airport(entry);status="LOADING "+entry.icao;
    s.menu=false;
    s.initial_dropdown=s.airport_dropdown=false;
}
UiModel ui_model() {
    UiModel m;m.index_ready=index_ready;m.profile=profile_ok;m.manual=manual_airport;
    m.bottom_inset=status_bar_height();
    m.database=airport_database_status;
    m.status=status;
    const auto& plan=fms_port.sampler.plan();m.fms_status=fms_port.sampler.status();
    if(plan.observed&&index_ready) {
        if(find_airport(airport_index,plan.origin.airport))m.fms_origin=plan.origin.airport;
        if(find_airport(airport_index,plan.destination.airport))m.fms_destination=plan.destination.airport;
        if(find_airport(airport_index,plan.alternate))m.fms_alternate=plan.alternate;
    }
    if(airport) {m.code=(airport->icao_explicit?"":"IDENT ")+airport->icao+(airport->iata.empty()?"":" "+airport->iata);m.name=airport->name;m.runways=static_cast<int>(airport->runways.size());
        m.pavements=static_cast<int>(airport->pavements.size());m.stands=static_cast<int>(airport->stands.size());m.taxiways=static_cast<int>(airport->edges.size());
        if(a350_map){const auto st=a350_map->stats();m.terminals=st.terminals;m.buildings=st.buildings;}}
    if(index_ready)m.airports=static_cast<int>(airport_index.size());
    for(int i=0;i<4;++i)m.features[i]=&map_features[i];
    if(airport&&runway_index>=0)m.runway=airport->runways[runway_index].name;
    return m;
}
void ui_action(int id,UiAction action,int arg=0) {
    auto& s=views[id];s.menu=false;s.wheel={};
    switch(action) {
        case UiAction::map_data:open_panel(id,Panel::map_data);break;
        case UiAction::airport:open_panel(id,Panel::airport);break;
        case UiAction::status:open_panel(id,Panel::status);break;
        case UiAction::close:open_panel(id,Panel::closed);break;
        case UiAction::focus:begin_airport_modification(s);s.initial_dropdown=s.airport_dropdown=false;focus_airport(id);break;
        case UiAction::clear:begin_airport_modification(s);s.query.clear();refresh_search(id);focus_airport(id);break;
        case UiAction::backspace:if(!s.query.empty()) {begin_airport_modification(s);s.query.pop_back();}refresh_search(id);focus_airport(id);break;
        case UiAction::character:
            if(s.query.size()<40) {begin_airport_modification(s);s.query+=static_cast<char>(std::toupper(static_cast<unsigned char>(arg)));refresh_search(id);}
            focus_airport(id);break;
        case UiAction::result:
            if(arg>=0&&arg<int(s.results.size())) {
                s.selection=arg;confirm_airport_candidate(s);s.airport_dropdown=s.initial_dropdown=false;release_input();
            }break;
        case UiAction::previous_page:scroll_airports(s,-3);break;
        case UiAction::next_page:scroll_airports(s,3);break;
        case UiAction::display_airport:display_airport(id);break;
        case UiAction::fms_airport: {
            const auto model=ui_model();const auto code=arg==0?model.fms_origin:arg==1?model.fms_destination:model.fms_alternate;
            if(code.empty())break;
            std::optional<IndexEntry> entry;
            {std::lock_guard<std::mutex> lock(db_mutex);entry=find_airport(airport_index,code);}
            if(entry) {s.query.clear();s.results={*entry};s.selection=0;display_airport(id);}
            break;
        }
        case UiAction::nearest:nearest_airport();s.panel=Panel::closed;break;
        case UiAction::runway_previous:select_runway(runway_index-1);break;
        case UiAction::runway_next:select_runway(runway_index+1);break;
        case UiAction::zoom_in:zoom(s,true);break;
        case UiAction::zoom_out:zoom(s,false);break;
        case UiAction::center:s.follow=true;break;
        case UiAction::add_cross:if(s.crosses.size()<100)s.crosses.push_back(s.context_world);break;
        case UiAction::add_flag:if(s.flags.size()<100)s.flags.push_back(s.context_world);break;
        case UiAction::erase_crosses:s.crosses.clear();break;
        case UiAction::erase_flags:s.flags.clear();break;
        case UiAction::orient:s.orientation=s.orientation==1?-1:s.orientation+1;break;
        case UiAction::report:diagnostics();break;
        case UiAction::none:case UiAction::dropdown_background:case UiAction::dropdown_thumb:case UiAction::landing_shift:break;
        case UiAction::airport_scroll:scroll_airports(s,arg);break;
        case UiAction::initial_scroll:scroll_initials(s,ui_model(),false,arg);break;
        case UiAction::panel_toggle:open_panel(id,s.panel==Panel::closed?s.last_panel:Panel::closed);break;
        case UiAction::keyboard_toggle:s.keyboard=!s.keyboard;if(s.keyboard)focus_airport(id);else release_input();break;
        case UiAction::feature_kind:
            s.feature_kind=std::clamp(arg,0,3);s.feature_selection=-1;s.feature_first=0;s.feature_initial='-';
            s.feature_dropdown=s.feature_initial_dropdown=false;break;
        case UiAction::feature_list:
            if(s.feature_kind>=0&&s.feature_kind<4&&!map_features[s.feature_kind].empty()) {
                s.feature_dropdown=!s.feature_dropdown;s.feature_initial_dropdown=false;
                if(s.feature_selection>=0)s.feature_first=s.feature_selection;
                scroll_map_features(s,ui_model(),0);
            }break;
        case UiAction::feature_select:
            if(s.feature_kind>=0&&s.feature_kind<4&&arg>=0&&arg<int(map_features[s.feature_kind].size())) {
                s.feature_selection=arg;s.context_world=map_features[s.feature_kind][arg].pos;s.feature_dropdown=false;
                const auto& name=map_features[s.feature_kind][arg].name;
                const char c=name.empty()?'#':static_cast<char>(std::toupper(static_cast<unsigned char>(name[0])));
                s.feature_initial=s.feature_kind==0?'-':c>='A'&&c<='Z'?c:'#';
            }break;
        case UiAction::feature_center:
            if(s.feature_kind>=0&&s.feature_kind<4&&s.feature_selection>=0&&s.feature_selection<int(map_features[s.feature_kind].size())) {s.center=map_features[s.feature_kind][s.feature_selection].pos;s.follow=false;}break;
        case UiAction::feature_previous:scroll_map_features(s,ui_model(),-3);break;
        case UiAction::feature_next:scroll_map_features(s,ui_model(),3);break;
        case UiAction::feature_scroll:scroll_map_features(s,ui_model(),arg);break;
        case UiAction::feature_initial_scroll:scroll_initials(s,ui_model(),true,arg);break;
        case UiAction::feature_initial_list:
            if(s.feature_kind>0&&s.feature_kind<4&&!map_features[s.feature_kind].empty()) {
                s.feature_initial_dropdown=!s.feature_initial_dropdown;s.feature_dropdown=false;
                const auto letters=map_feature_initials(s,ui_model());
                const auto at=letters.find(s.feature_initial);
                s.feature_initial_first=at==std::string::npos?0:int(at);
                scroll_initials(s,ui_model(),true,0);s.wheel={};
            }break;
        case UiAction::feature_initial_select:locate_map_feature_initial(s,ui_model(),static_cast<char>(arg));break;
        case UiAction::airport_mode: {
            const auto code=s.selection>=0&&s.selection<int(s.results.size())?s.results[s.selection].icao:"";
            begin_airport_modification(s);release_input();s.airport_mode=std::clamp(arg,0,2);s.query.clear();s.initial_dropdown=s.airport_dropdown=false;
            {std::lock_guard<std::mutex> lock(db_mutex);
                select_airport_candidate(s,airport_index,code);search_airports(s,airport_index,0,0);}
            break;
        }
        case UiAction::initial_list: {
            begin_airport_modification(s,true);release_input();s.initial_dropdown=!s.initial_dropdown;s.airport_dropdown=false;
            const auto at=s.airport_initials.find(s.initial);
            s.initial_first=at==std::string::npos?0:int(at);
            scroll_initials(s,ui_model(),false,0);s.wheel={};break;
        }
        case UiAction::airport_list:
            begin_airport_modification(s);release_input();s.query.clear();refresh_search(id);s.airport_dropdown=!s.airport_dropdown;s.initial_dropdown=false;break;
        case UiAction::initial_previous:case UiAction::initial_next:case UiAction::initial_select: {
            begin_airport_modification(s);release_input();s.query.clear();
            const auto& letters=s.airport_initials;
            if(letters.empty())break;
            auto at=letters.find(s.initial);if(at==std::string::npos)at=0;
            if(action==UiAction::initial_previous&&at>0)s.initial=letters[at-1];
            else if(action==UiAction::initial_next&&at+1<letters.size())s.initial=letters[at+1];
            else if(action==UiAction::initial_select&&letters.find(static_cast<char>(arg))!=std::string::npos)s.initial=static_cast<char>(arg);
            refresh_search(id);locate_airport_initial(s,s.initial);
            s.initial_dropdown=false;s.airport_dropdown=true;break;
        }
    }
}

void graphics() {XPLMSetGraphicsState(0,0,0,0,1,0,0);}
void restore_graphics(const int* s,int texture) {
    XPLMSetGraphicsState(s[0],s[1],s[2],s[3],s[4],s[5],s[6]);
    XPLMBindTexture2d(texture,0);
}
void color(const float* c) {glColor3d(c[0]*render_gain,c[1]*render_gain,c[2]*render_gain);}
void line(Vec2 a,Vec2 b,const float* c,float width=1) {graphics();color(c);glLineWidth(width);glBegin(GL_LINES);glVertex2d(a.x,a.y);glVertex2d(b.x,b.y);glEnd();}
void rect(double l,double b,double r,double t,const float* c) {graphics();color(c);glBegin(GL_QUADS);glVertex2d(l,b);glVertex2d(r,b);glVertex2d(r,t);glVertex2d(l,t);glEnd();}
void text(double x,double y,const std::string& s,const float* c=white,double max_width=-1,double size=18,bool ellipsis=true) {
    float rgb[3]={static_cast<float>(c[0]*render_gain),static_cast<float>(c[1]*render_gain),static_cast<float>(c[2]*render_gain)};
    if(map_text&&map_font.ready()&&map_font.upload(XPLMGenerateTextureNumbers,XPLMBindTexture2d)) {
        XPLMSetGraphicsState(0,1,0,0,1,0,0);XPLMBindTexture2d(map_font.texture(),0);
        map_font.draw(x,y,s,rgb,max_width<0?std::max(0.0,594-x):max_width,size,ellipsis);graphics();return;
    }
    XPLMDrawString(rgb,static_cast<int>(x),static_cast<int>(y),s.c_str(),nullptr,xplmFont_Basic);
}
using MapTransform=MapProjection;
MapTransform transform(int id) {
    const auto& view=views[id];MapTransform m;
    int mode=id<2?static_cast<int>(value("mode"+std::to_string(id))):4;
    if(airport) {
        Vec2 ac=local(value("lat"),value("lon"),airport->lat,airport->lon);
        // Follow ownship across the airport, irrespective of distance from
        // the first runway endpoint used as the local coordinate origin.
        if(aircraft_position_ready(id<2?id:-1)&&(mode!=4||!manual_airport||length(ac)<60000))m.center=ac;
        else if(runway_index>=0) {const auto& r=airport->runways[runway_index];m.center=(r.start+r.end)*.5;}
    }
    if(!view.follow)m.center=view.center;
    bool heading=view.orientation<0?(mode==2||mode==3):view.orientation==1;
    const auto viewport=map_viewport(mode,view.panel!=Panel::closed,panel_height(view.panel));
    m.anchor=viewport.anchor;m.radius=viewport.radius;m.bottom=viewport.bottom;
    m.bottom+=status_bar_height();
    m.scale=m.radius/(view.range_nm*1852);m.angle=heading?value("psi")*pi/180:0;return m;
}
void draw_controls(int id) {
    OANS_TRACE_SCOPE();
    const float fill[]={.102f,.102f,.102f},edge[]={.55f,.55f,.55f},selected[]={.235f,.235f,.235f},dim[]={.65f,.65f,.68f};
    auto frame=nd_controls(views[id],ui_model());
    const float status_bg[]={.40f,.40f,.48f},yellow[]={1,1,0},black[]={0,0,0};
    auto selector_text=[&](const UiControl& c,double x,double y,double width,double cap,const float* normal) {
        if(c.enabled&&c.highlight) {
            rect(x-2,y-2,x+std::min(width,map_font.width(c.label,cap))+2,y+cap+2,cyan);
            text(x,y,c.label,black,width,cap);
        } else text(x,y,c.label,c.enabled?(c.pending?yellow:normal):dim,width,cap);
    };
    for(const auto& b:frame.backgrounds)rect(b.l,b.b,b.r,b.t,frame.status_style?status_bg:selected);
    for(const auto& b:frame.rules)rect(b.l,b.b,b.r,b.t,white);
    // Expanded CITY menu must cover the airport details underneath it.
    for(const auto& t:frame.texts)text(t.at.x,t.at.y,t.text,t.colour==1?amber:t.colour==2?green:t.colour==3?yellow:white,t.width,t.size);
    for(const auto& c:frame.controls) {
        const auto& b=c.box;
        if(c.action==UiAction::close&&c.label=="X") {
            rect(b.l,b.b,b.r,b.t,white);rect(b.l+1,b.b+1,b.r-1,b.t-1,status_bg);
            graphics();glColor3fv(white);glLineWidth(2);glBegin(GL_LINES);
            glVertex2d(b.l+5,b.b+5);glVertex2d(b.r-5,b.t-5);
            glVertex2d(b.l+5,b.t-5);glVertex2d(b.r-5,b.b+5);glEnd();continue;
        }
        if(c.action==UiAction::dropdown_background) {
            rect(b.l,b.b,b.r,b.t,white);rect(b.l+1,b.b+1,b.r-1,b.t-1,status_bg);continue;
        }
        if(c.action==UiAction::airport_scroll||c.action==UiAction::initial_scroll||c.action==UiAction::feature_scroll||c.action==UiAction::feature_initial_scroll||c.action==UiAction::dropdown_thumb) {
            rect(b.l,b.b,b.r,b.t,dim);
            if(c.action==UiAction::dropdown_thumb) {
                rect(b.l,b.b,b.r,b.t,white);rect(b.l+1,b.b+1,b.r-1,b.t-1,status_bg);
            }
            continue;
        }
        if(c.action==UiAction::result||c.action==UiAction::initial_select||c.action==UiAction::feature_select||c.action==UiAction::feature_initial_select) {
            rect(b.l,b.b,b.r,b.t,c.selected?cyan:status_bg);
            if(c.selected)rect(b.l+2,b.b+2,b.r-2,b.t-2,status_bg);
            text(b.l+5,(b.b+b.t)/2-6,c.label,white,b.r-b.l-10,12,false);continue;
        }
        const bool dropdown=c.action==UiAction::initial_list||c.action==UiAction::feature_list||c.action==UiAction::feature_initial_list;
        const bool arrow_button=c.action==UiAction::airport_list||c.action==UiAction::panel_toggle||(c.action==UiAction::close&&c.label!="X")||c.action==UiAction::initial_previous||c.action==UiAction::initial_next;
        if(dropdown||arrow_button) {
            // Reference dropdown: white outline, dark value field, purple-grey
            // arrow compartment and a white filled triangle (no text glyph).
            const float* border=c.enabled?white:dim;
            rect(b.l,b.b,b.r,b.t,border);
            rect(b.l+2,b.b+2,b.r-2,b.t-2,dropdown?fill:status_bg);
            const double split=dropdown?b.r-(c.action==UiAction::initial_list||c.action==UiAction::feature_initial_list?20:24):b.l;
            if(dropdown) {
                rect(split,b.b+2,b.r-2,b.t-2,status_bg);
                rect(split,b.b+2,split+1,b.t-2,border);
                const double cap=12,width=map_font.width(c.label,cap);
                selector_text(c,b.l+std::max(4.0,(split-b.l-width)/2),(b.b+b.t)/2-cap/2,split-b.l-8,cap,green);
            }
            const double x=(split+b.r)/2,y=(b.b+b.t)/2;
            graphics();glColor3fv(border);glBegin(GL_TRIANGLES);
            if(c.action==UiAction::initial_previous) {glVertex2d(x+3,y+6);glVertex2d(x+3,y-6);glVertex2d(x-5,y);}
            else if(c.action==UiAction::initial_next) {glVertex2d(x-3,y+6);glVertex2d(x-3,y-6);glVertex2d(x+5,y);}
            else if(c.action==UiAction::panel_toggle&&views[id].panel==Panel::closed) {glVertex2d(x-6,y-3);glVertex2d(x+6,y-3);glVertex2d(x,y+5);}
            else {glVertex2d(x-6,y+3);glVertex2d(x+6,y+3);glVertex2d(x,y-5);}
            glEnd();continue;
        }
        if(c.action==UiAction::airport_mode||c.action==UiAction::feature_kind) {
            const float* colour=c.selected?cyan:white;
            graphics();
            if(c.selected) {
                glColor3fv(cyan);glBegin(GL_TRIANGLE_FAN);
                glVertex2d(b.l+8,(b.b+b.t)/2);
                for(int i=0;i<=24;++i) {double a=i*2*pi/24;glVertex2d(b.l+8+7*std::cos(a),(b.b+b.t)/2+7*std::sin(a));}glEnd();
            }
            glColor3fv(white);glLineWidth(1);glBegin(GL_LINE_LOOP);
            for(int i=0;i<24;++i) {double a=i*2*pi/24;glVertex2d(b.l+8+7*std::cos(a),(b.b+b.t)/2+7*std::sin(a));}glEnd();
            text(b.l+24,(b.b+b.t)/2-6,c.label,colour,b.r-b.l-28,12);continue;
        }
        if(!frame.status_style||!c.selected) {
            rect(b.l,b.b,b.r,b.t,frame.status_style?white:edge);
            const bool map_button=views[id].panel==Panel::map_data&&(c.action==UiAction::add_cross||c.action==UiAction::add_flag||c.action==UiAction::landing_shift||c.action==UiAction::feature_center);
            rect(b.l+1,b.b+1,b.r-1,b.t-1,frame.status_style?(c.action==UiAction::close||map_button?status_bg:fill):c.selected?selected:fill);
        } else {
            // Every selected item owns an opaque fill; the airport tab is
            // above the bottom status strip and otherwise exposes the map.
            rect(b.l,b.b,b.r,b.t,status_bg);
        }
        const auto newline=c.label.find('\n');
        if(newline!=std::string::npos) {
            const std::string rows[]={c.label.substr(0,newline),c.label.substr(newline+1)};
            for(int row=0;row<2;++row) {
                const double width=map_font.width(rows[row],12);
                text(b.l+std::max(4.0,(b.r-b.l-width)/2),(b.b+b.t)/2+1-row*14,rows[row],c.enabled?white:dim,b.r-b.l-8,12);
            }
            continue;
        }
        double cap=frame.status_style?(c.action==UiAction::close?14:12):c.label.size()>20?14:16;
        double width=map_font.width(c.label,cap),x=b.l+std::max(5.0,(b.r-b.l-width)*.5);
        const bool selector=c.action==UiAction::focus||c.action==UiAction::airport_list||c.action==UiAction::initial_list||c.action==UiAction::initial_select||c.action==UiAction::result;
        selector_text(c,x,(b.b+b.t)/2-cap*.5,b.r-x-5,cap,selector&&views[id].panel==Panel::airport?green:c.selected?cyan:white);
        if(c.selected&&!frame.status_style)rect(b.l+1,b.t-2,b.r-1,b.t-1,cyan);
    }
}
void draw_compass(int id) {
    OANS_TRACE_SCOPE();
    const auto m=transform(id);int mode=id<2?static_cast<int>(value("mode"+std::to_string(id))):4;
    const bool arc=mode==3,plan=mode==4;double radius=m.radius;
    const float ring[]={.90f,.90f,.90f};
    auto point=[&](double a,double r){a*=pi/180;return m.anchor+Vec2{r*std::sin(a),r*std::cos(a)};};
    auto curve=[&](double r,bool dashed){double low=arc?-62:0,high=arc?62:360;
        for(double a=low;a<high;a+=2)if(!dashed||int((a-low)/2)%3!=2)line(point(a,r),point(std::min(a+2,high),r),ring,1);};
    curve(radius,false);curve(radius*.5,true);
    auto caption=[&](Vec2 p,const std::string& s,double rotation,double cap){
        glMatrixMode(GL_MODELVIEW);glPushMatrix();glTranslated(p.x,p.y,0);glRotated(rotation,0,0,1);
        text(-map_font.width(s,cap)*.5,-cap*.5,s,white,70,cap);glPopMatrix();};
    if(plan) {
        const char* cardinal[]={"N","E","S","W"};for(int k=0;k<4;++k)caption(point(k*90,radius-18),cardinal[k],0,20);
    } else {
        double heading=value("magpsi"),start=arc?std::ceil((heading-60)/5)*5:0,stop=arc?heading+60:355;
        for(double degree=start;degree<=stop;degree+=5) {
            double a=degree-heading;if(!arc)a=std::remainder(a,360.0);
            int d=(static_cast<int>(degree)%360+360)%360;bool major=d%10==0;
            line(point(a,radius),point(a,radius-(major?13:7)),ring,1.2f);
            if((arc&&major)||(!arc&&d%30==0))caption(point(a,radius+(arc?20:-25)),std::to_string(d/10),-a,d%30==0?21:16);
        }
        line(point(0,radius-15),point(0,radius+15),taxi_yellow,2);
    }
    std::ostringstream half;half<<views[id].range_nm*.5;
    for(int sign:{-1,1}) {auto p=point(sign*52,radius*.5+12);text(p.x-14,p.y,half.str(),cyan,65,16);}
}
std::string advisory_fms_runway() {
    const auto& p=fms_port.sampler.plan();
    const auto* source=displayed_advisory_airport();
    if(!p.observed||!source)return {};
    if(p.origin.airport==source->icao)return p.origin.runway;
    if(p.destination.airport==source->icao)return p.destination.runway;
    return {};
}
bool advisory_display_available(int side,bool pfd) {
    if(!enabled||!profile_ok||!aircraft_ok||!config.nd||displayed_advisories().empty())return false;
    if(!dr("fallback").numeric()||value("swap"+std::to_string(side))!=0||value("ecamSwap")!=0)return false;
    if(!pfd) {const int mode=static_cast<int>(value("mode"+std::to_string(side),0,-1));if(mode<2||mode>4)return false;}
    const int brightness=pfd?profile::current.pfd_brightness[side]:profile::current.nd_brightness[side];
    return dr("bright").numeric(brightness)&&value("bright",brightness)>=.01&&
        dr("bus").numeric(1)&&std::max(value("bus",0),value("bus",1))>=20;
}
void draw_advisory(int id,bool pfd=false) {
    OANS_TRACE_SCOPE();
    if(!a350_ui||displayed_advisories().empty())return;
    const int mode=id<2?static_cast<int>(value("mode"+std::to_string(id))):4;
    if(!map_font.upload(XPLMGenerateTextureNumbers,XPLMBindTexture2d))throw std::runtime_error("Advisory font upload failed");
    const auto& vertices=a350_ui->advisory(mode,pfd,displayed_advisories(),advisory_fms_runway(),advisory_blink());
    XPLMSetGraphicsState(0,1,0,0,1,0,0);XPLMBindTexture2d(map_font.texture(),0);
    draw_a350_vertices(vertices,render_gain);graphics();
}
void draw_map(int id,bool nd) {
    OANS_TRACE_SCOPE();
    const float bg[]={0,0,0},pave[]={.243f,.243f,.243f};
    rect(0,0,600,600,bg);
    auto m=transform(id);const auto& view=views[id];
    if(config.test&&nd) {
        rect(5,5,595,595,pave);rect(10,10,590,590,bg);
        for(int i=0;i<=5;++i) {double p=20+i*112;line({p,20},{p,580},cyan);line({20,p},{580,p},cyan);}
        line({0,0},{600,600},amber);line({0,600},{600,0},amber);
        text(40,535,"TOLISS OANS ND / CLICK TEST",green);text(40,505,"CLICK 4 CORNERS AND CENTER",cyan);
        text(40,475,"MAP TOGGLE IN PLUGINS MENU");
    } else if(airport) {
        if(!a350_map)throw std::runtime_error("A350 map engine is not loaded");
        if(!map_font.upload(XPLMGenerateTextureNumbers,XPLMBindTexture2d))
            throw std::runtime_error("A350 font atlas upload failed");
        A350View input;
        input.center=m.center;input.anchor=m.anchor;input.scale=m.scale;input.angle=m.angle;
        input.radius=m.radius;input.bottom=m.bottom;
        input.lat=value("lat");input.lon=value("lon");input.psi=value("psi");input.mag=value("magpsi");
        input.simple_stand_names=config.simple_stand_names;
        input.position_valid=aircraft_position_ready(id<2?id:-1);
        if(input.position_valid)input.traffic=traffic_targets;
        input.mode=id<2?static_cast<int>(value("mode"+std::to_string(id))):4;input.range=view.range_nm;
        if(view.feature_kind==0&&view.feature_selection>=0&&view.feature_selection<int(map_features[0].size()))
            input.map_runway=map_features[0][view.feature_selection].name;
        input.crosses=view.crosses;input.flags=view.flags;
        if(displayed_advisory_airport()&&airport->icao==displayed_advisory_airport()->icao) {
            input.runway_ahead=displayed_advisories();input.alert_blink=advisory_blink();
        }
        const auto& plan=fms_port.sampler.plan();
        input.outside_plan=airport_outside_active_plan(plan,airport->icao);
        if(plan.observed) {
            const bool ground=value("ground",0)>0||value("ground",1)>0||value("ground",2)>0;
            if(airport->icao==plan.origin.airport&&(ground||airport->icao!=plan.destination.airport)) {
                input.fms_airport=plan.origin.airport;input.fms_runway=plan.origin.runway;
            } else if(airport->icao==plan.destination.airport) {
                input.fms_airport=plan.destination.airport;input.fms_runway=plan.destination.runway;
            }
        }
        const auto& vertices=a350_map->render_base(id,input,XPLMGetElapsedTime(),captured_view==id||input_owner==id,XPLMGetCycleNumber());
        if(texture_maps[id].lock()!=a350_map) {map_textures[id].invalidate();texture_maps[id]=a350_map;}
        XPLMSetGraphicsState(0,1,0,0,1,0,0);XPLMBindTexture2d(map_font.texture(),0);
        if(map_textures[id].update(vertices,map_font.texture(),a350_map->cache_stats(id).builds,XPLMGenerateTextureNumbers))
            map_textures[id].draw(render_gain,XPLMBindTexture2d,a350_map->base_view(id),input);
        else MapTexture::draw_fallback(vertices,render_gain,a350_map->base_view(id),input);
        const auto& dynamic=a350_map->render_dynamic(input);
        XPLMBindTexture2d(map_font.texture(),0);draw_a350_vertices(dynamic,render_gain);graphics();
    } else {
        const bool loading=airport_map_loading;
        const std::string message=loading?"PLEASE WAIT":"ARPT NAV NOT AVAIL";
        text(300-map_font.width(message,22)*.5,300,message,loading?white:amber,-1,22);
    }
    // Flight information and compact airport header follow the A350 hierarchy.
    rect(0,552,600,600,bg);
    const std::string airport_label=airport?((airport->icao_explicit?"":"IDENT ")+airport->icao+
        (airport->iata.empty()?"":"  "+airport->iata)):"----";
    std::ostringstream range;range<<view.range_nm;
    text(8,578,"GS",white,40,16);text(47,576,std::to_string(static_cast<int>(value("speed")/kt+.5)),green,65,22);
    text(110,578,"TAS",white,48,16);text(165,578,value("tas")/kt>=60?std::to_string(static_cast<int>(value("tas")/kt+.5)):"---",green,70,16);
    const int mode=id<2?static_cast<int>(value("mode"+std::to_string(id))):4;
    const bool heading=view.orientation<0?(mode==2||mode==3):view.orientation==1;
    std::string title=airport?airport->name:"NO AIRPORT";for(char& c:title)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    double title_width=std::min(290.0,map_font.width(title,16));text(592-title_width,579,title,white,290,16);
    text(592-map_font.width(airport_label,18),554,airport_label,white,220,18);
    text(8,554,heading?"HDG UP":"NORTH UP",white,170,15);
    text(8,44+status_bar_height(),range.str()+" NM",cyan,110,16);
    if(advisory_test.active)text(125,44+status_bar_height(),"TEST",magenta,80,16);
    // Clear the strip before controls so its relocated arrow stays visible.
    if(status_bar_height()>0)rect(0,0,600,status_bar_height(),bg);
    if(!(config.test&&nd))draw_controls(id);
    if(a350_ui&&!(config.test&&nd)) {
        const auto& notice=a350_ui->plan_notice(mode,airport&&airport_outside_active_plan(fms_port.sampler.plan(),airport->icao));
        XPLMSetGraphicsState(0,1,0,0,1,0,0);XPLMBindTexture2d(map_font.texture(),0);
        draw_a350_vertices(notice,render_gain);graphics();
    }
    if(status_bar_height()>0) {
        const float yellow[]={1,1,0};
        if(!airport_map_loading&&aircraft_position_ready(id<2?id:-1)&&!current_surface.empty()) {
            const double width=map_font.width(current_surface,16);
            rect(300-width*.5-5,stus_bar_safe_bottom,300+width*.5+5,status_bar_height()-1,yellow);
            rect(300-width*.5-3,stus_bar_safe_bottom+2,300+width*.5+3,status_bar_height()-3,bg);
            text(300-width*.5,13,current_surface,yellow,width,16,false);
        }
    }
}
// Convert the current virtual map rectangle to framebuffer scissor coordinates.
void scissor_map() {
    GLdouble model[16],proj[16],x[2],y[2],z;GLint viewport[4],previous[4];
    glGetDoublev(GL_MODELVIEW_MATRIX,model);glGetDoublev(GL_PROJECTION_MATRIX,proj);glGetIntegerv(GL_VIEWPORT,viewport);
    gluProject(0,0,0,model,proj,viewport,&x[0],&y[0],&z);gluProject(600,600,0,model,proj,viewport,&x[1],&y[1],&z);
    int l=static_cast<int>(std::floor(std::min(x[0],x[1]))),b=static_cast<int>(std::floor(std::min(y[0],y[1])));
    int r=static_cast<int>(std::ceil(std::max(x[0],x[1]))),t=static_cast<int>(std::ceil(std::max(y[0],y[1])));
    if(glIsEnabled(GL_SCISSOR_TEST)) {glGetIntegerv(GL_SCISSOR_BOX,previous);l=std::max(l,previous[0]);b=std::max(b,previous[1]);r=std::min(r,previous[0]+previous[2]);t=std::min(t,previous[1]+previous[3]);}
    glEnable(GL_SCISSOR_TEST);glScissor(l,b,std::max(0,r-l),std::max(0,t-b));
}
void probe_nd(int id) {
    if(!panel_probe_requested||!dr("fbo").numeric())return;
    panel_probe_requested=false;
    using BindFramebuffer=void (APIENTRY*)(GLenum,GLuint);
    using BindBuffer=void (APIENTRY*)(GLenum,GLuint);
    auto bind_fbo=gl_proc<BindFramebuffer>("glBindFramebuffer");auto bind_buffer=gl_proc<BindBuffer>("glBindBuffer");
    if(!bind_fbo||!bind_buffer) {log("ND PROBE unavailable: framebuffer APIs missing");return;}
    GLdouble model[16],projection[16],x,y,z;GLint viewport[4],previous_read=0,pack=0,origin=0;
    glGetDoublev(GL_MODELVIEW_MATRIX,model);glGetDoublev(GL_PROJECTION_MATRIX,projection);glGetIntegerv(GL_VIEWPORT,viewport);
    if(!gluProject(300,300,0,model,projection,viewport,&x,&y,&z))return;
    if(gl_proc<void(APIENTRY*)(GLenum,GLenum)>("glClipControl"))glGetIntegerv(0x935C,&origin);
    glGetIntegerv(0x8CAA,&previous_read);glGetIntegerv(0x88ED,&pack);
    const auto target=static_cast<GLuint>(value("fbo"));
    // Only the SDK-provided active target is read, within this draw callback.
    // Preserve the caller's read target and pixel pack binding.
    if(static_cast<GLuint>(previous_read)!=target)bind_fbo(0x8CA8,target);
    if(pack)bind_buffer(0x88EB,0);
    unsigned char before[4]={},after[4]={},flipped[4]={};
    int ix=std::clamp(static_cast<int>(x),viewport[0],viewport[0]+viewport[2]-1);
    int iy=std::clamp(static_cast<int>(y),viewport[1],viewport[1]+viewport[3]-1);
    int flipped_y=2*viewport[1]+viewport[3]-iy-1;
    glReadPixels(ix,iy,1,1,GL_RGBA,GL_UNSIGNED_BYTE,before);
    glPushAttrib(GL_SCISSOR_BIT);glDisable(GL_SCISSOR_TEST);
    const float marker[]={1,0,1};rect(292,292,308,308,marker);glPopAttrib();
    glReadPixels(ix,iy,1,1,GL_RGBA,GL_UNSIGNED_BYTE,after);
    glReadPixels(ix,flipped_y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,flipped);
    auto error=glGetError();
    if(pack)bind_buffer(0x88EB,static_cast<GLuint>(pack));
    if(static_cast<GLuint>(previous_read)!=target)bind_fbo(0x8CA8,static_cast<GLuint>(previous_read));
    std::ostringstream s;s<<"ND PROBE side="<<id<<" fbo="<<target<<" read_before="<<previous_read<<" pack="<<pack
        <<" reverse_y="<<value("reverseY")<<" clip_origin="<<origin<<" xy="<<ix<<','<<iy
        <<" rgb_before="<<int(before[0])<<','<<int(before[1])<<','<<int(before[2])
        <<" rgb_after="<<int(after[0])<<','<<int(after[1])<<','<<int(after[2])
        <<" rgb_flipped="<<int(flipped[0])<<','<<int(flipped[1])<<','<<int(flipped[2])<<" gl_error="<<error;log(s.str());
}
void render_rect(double l,double b,double width,double height,int id,bool nd) {
    OANS_TRACE_SCOPE();
    GLint matrix_mode;glGetIntegerv(GL_MATRIX_MODE,&matrix_mode);glMatrixMode(GL_MODELVIEW);glPushMatrix();
    glTranslated(l,b,0);glScaled(width/600,height/600,1);
    glPushAttrib(GL_SCISSOR_BIT|GL_LINE_BIT|GL_CURRENT_BIT);scissor_map();
    // Calibration geometry is bounded by the ND itself; omit our computed
    // scissor during this diagnostic to distinguish clipping from late redraw.
    if(nd&&config.test)glDisable(GL_SCISSOR_TEST);
    map_text=true;
    if(!nd||nd_on[id])draw_map(id,nd);
    if(nd&&a350_ui) {
        if(!map_font.upload(XPLMGenerateTextureNumbers,XPLMBindTexture2d))
            throw std::runtime_error("A350 checkbox font atlas upload failed");
        XPLMSetGraphicsState(0,1,0,0,1,0,0);XPLMBindTexture2d(map_font.texture(),0);
        draw_a350_vertices(a350_ui->oans_switch(nd_on[id]).vertices,render_gain);graphics();
    }
    if(!nd||advisory_display_available(id,false))draw_advisory(id);
    if(a350_ui&&(!nd||nd_on[id]))if(auto at=pointer_on_map(id)) {
        const auto& vertices=a350_ui->cursor(*at);
        XPLMSetGraphicsState(0,1,0,0,1,0,0);XPLMBindTexture2d(map_font.texture(),0);
        draw_a350_vertices(vertices,render_gain);graphics();
    }
    map_text=false;if(nd&&nd_on[id]&&config.test)probe_nd(id);glPopAttrib();
    OANS_TRACE("render_rect OpenGL submission done side="+std::to_string(id));
    glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(matrix_mode);
}
void render_advisory_rect(double l,double b,double width,double height,int side,bool pfd) {
    OANS_TRACE_SCOPE();
    GLint mode;glGetIntegerv(GL_MATRIX_MODE,&mode);glMatrixMode(GL_MODELVIEW);glPushMatrix();
    glTranslated(l,b,0);glScaled(width/600,height/600,1);
    glPushAttrib(GL_SCISSOR_BIT|GL_LINE_BIT|GL_CURRENT_BIT);scissor_map();
    draw_advisory(side,pfd);glPopAttrib();glMatrixMode(GL_MODELVIEW);glPopMatrix();glMatrixMode(mode);
}
const char* nd_overlay_blocked(int side) {
    if(!enabled)return "PLUGIN DISABLED";
    if(!aircraft_ok||!profile_ok)return "AIRCRAFT PROFILE UNAVAILABLE";
    if(!config.nd)return "ND DISABLED IN CONFIG";
    if(!dr("fallback").numeric())return "DISPLAY RESOLUTION INTERFACE MISSING";
    if(value("range"+std::to_string(side),0,99)!=0)return "SET PHYSICAL ND RANGE TO 10";
    const int mode=static_cast<int>(value("mode"+std::to_string(side),0,-1));
    if(!config.test&&(mode<2||mode>4))return "SELECT NAV / ARC / PLAN";
    if(value("map"+std::to_string(side))<.5)return "AIRCRAFT MAP UNAVAILABLE";
    if(value("bright",profile::current.nd_brightness[side])<.01)return "ND BRIGHTNESS OFF";
    if(value("swap"+std::to_string(side))!=0||value("ecamSwap")!=0)return "DISPLAY SWITCHING ACTIVE";
    if(dr("bus").has(1)&&std::max(value("bus",0),value("bus",1))<20)return "DISPLAY BUS BELOW 20V";
    return "";
}
const char* nd_blocked(int side) {
    const char* blocked=nd_overlay_blocked(side);
    if(blocked[0])return blocked;
    return nd_on[side]?"":"SIDE MAP SWITCH OFF";
}
bool nd_available(int side) {return nd_blocked(side)[0]=='\0';}
bool nd_overlay_available(int side) {return nd_overlay_blocked(side)[0]=='\0';}
int gauge_draw(XPLMDrawingPhase,int,void*) {
    OANS_TRACE_SCOPE();
    try {
        if(!enabled||XPLMGetElapsedTime()-heartbeat>1)return 1;
        const int rt=static_cast<int>(value("rt"));OANS_TRACE("Gauge panel_render_type="+std::to_string(rt)+" enabled="+std::to_string(enabled)+" profile="+std::to_string(profile_ok)+" aircraft="+std::to_string(aircraft_ok));if(rt>=0&&rt<=2)++pass_count[rt];
        if(rt!=1&&rt!=2)return 1;
        double l=value("pl"),b=value("pb"),w=value("pr")-l,h=value("pt")-b;
        auto& trace=panel_trace[rt];trace.time=XPLMGetElapsedTime();
        trace.bounds[0]=l;trace.bounds[1]=b;trace.bounds[2]=l+w;trace.bounds[3]=b+h;

#if OANS_DEBUG
        static bool gpu_logged=false;
        if(!gpu_logged) {
            gpu_logged=true;
            for(auto key:{GL_VENDOR,GL_RENDERER,GL_VERSION}) {
                OANS_TRACE("glGetString query="+std::to_string(key));const auto* text=glGetString(key);
                OANS_TRACE(std::string("OpenGL info=")+(text?reinterpret_cast<const char*>(text):"no current context"));
            }
        }
#endif
        OANS_TRACE("OpenGL panel state query begin");
        glGetIntegerv(GL_VIEWPORT,trace.viewport);glGetIntegerv(GL_POLYGON_MODE,trace.polygon);
        glGetIntegerv(0x8B8D,&trace.program);glGetIntegerv(0x85B5,&trace.vao);glGetIntegerv(0x8894,&trace.vbo);
        glGetIntegerv(0x8009,&trace.blend_equation); // GL_BLEND_EQUATION
        trace.cull=glIsEnabled(GL_CULL_FACE);trace.logic=glIsEnabled(GL_COLOR_LOGIC_OP);
        trace.stencil=glIsEnabled(GL_STENCIL_TEST);trace.scissor_on=glIsEnabled(GL_SCISSOR_TEST);
        glGetIntegerv(GL_SCISSOR_BOX,trace.scissor);
        OANS_TRACE("OpenGL panel state viewport="+std::to_string(trace.viewport[0])+","+std::to_string(trace.viewport[1])+","+std::to_string(trace.viewport[2])+","+std::to_string(trace.viewport[3])+" program="+std::to_string(trace.program)+" vao="+std::to_string(trace.vao)+" vbo="+std::to_string(trace.vbo)+" bounds="+std::to_string(l)+","+std::to_string(b)+","+std::to_string(w)+","+std::to_string(h));
        for(int side=0;side<2;++side)trace.blocked[side]=nd_blocked(side);
        // Panel preview confirms authored +V maps directly to these rows.
        // is_reverse_y describes 3-D drawing and can be stale in panel callbacks.
        auto target=texture_rectangle();
        if(!target||trace.viewport[2]<100||trace.viewport[3]<100) {
            for(auto& reason:trace.blocked)if(reason.empty())reason="INVALID PANEL TEXTURE TRANSFORM";
            return 1;
        }
        l=target->l;b=target->b;w=target->w;h=target->h;
        trace.target[0]=l;trace.target[1]=b;trace.target[2]=l+w;trace.target[3]=b+h;
        bool draw_needed=false;
        for(int side=0;side<2;++side)
            draw_needed=draw_needed||nd_overlay_available(side)||
                (rt==2&&(advisory_display_available(side,false)||advisory_display_available(side,true)));
        if(!draw_needed)return 1;
        OANS_TRACE("OpenGL capture/setup begin");
        DrawingState drawing_state(graphics,restore_graphics);
        OANS_TRACE("OpenGL capture/setup done");
        int resolution=value("fallback")>.5?1:0;
        for(int side=0;side<2;++side)if(nd_overlay_available(side)) {
            OANS_TRACE("ND geometry side="+std::to_string(side)+" resolution="+std::to_string(resolution));
            const auto& uv=profile::current.nd_uv[resolution][side];
            GLdouble model[16],projection[16],z;
            glGetDoublev(GL_MODELVIEW_MATRIX,model);glGetDoublev(GL_PROJECTION_MATRIX,projection);
            gluProject(l+uv[0]*w,b+uv[1]*h,0,model,projection,trace.viewport,&trace.projected[side][0],&trace.projected[side][1],&z);
            gluProject(l+uv[2]*w,b+uv[3]*h,0,model,projection,trace.viewport,&trace.projected[side][2],&trace.projected[side][3],&z);
            if(rt==1) {if(nd_on[side]) {const float black[3]={0,0,0};rect(l+uv[0]*w,b+uv[1]*h,l+uv[2]*w,b+uv[3]*h,black);}}
            else {
                render_gain=std::clamp(value("bright",profile::current.nd_brightness[side]),0.0,1.0);
                render_rect(l+uv[0]*w,b+uv[1]*h,(uv[2]-uv[0])*w,(uv[3]-uv[1])*h,side,true);render_gain=1;
            }
            ++trace.submitted[side];
        }
        if(rt==2)for(int side=0;side<2;++side) {
            for(bool pfd:{false,true})if(advisory_display_available(side,pfd)&&(pfd||!nd_overlay_available(side))) {
                const auto& uv=pfd?profile::current.pfd_uv[resolution][side]:profile::current.nd_uv[resolution][side];
                render_gain=std::clamp(value("bright",pfd?profile::current.pfd_brightness[side]:profile::current.nd_brightness[side]),0.0,1.0);
                render_advisory_rect(l+uv[0]*w,b+uv[1]*h,(uv[2]-uv[0])*w,(uv[3]-uv[1])*h,side,pfd);render_gain=1;
            }
        }
    } catch(const std::exception& e) {map_text=false;render_gain=1;config.nd=false;status="ND DRAW ERROR";log(e.what());}
    return 1;
}
std::pair<UiAction,int> map_label(int id,Vec2 p) {
    if(!airport)return {UiAction::none,-1};
    // Map gestures are handled by OANS controls.
    auto m=transform(id);double best=35;int runway=-1;
    for(size_t i=0;i<airport->runways.size();++i) {double d=length(m.to(airport->runways[i].start)-p);if(d<best) {best=d;runway=static_cast<int>(i);}}
    return runway>=0?std::make_pair(UiAction::runway_next,runway):std::make_pair(UiAction::none,-1);
}
std::array<double,3> rotate_point(const double* p) {
    const double psi=value("psi")*pi/180,t=value("theta")*pi/180,f=value("phi")*pi/180;
    double x=p[0]*std::cos(f)+p[1]*std::sin(f),y=-p[0]*std::sin(f)+p[1]*std::cos(f);
    double y2=y*std::cos(t)-p[2]*std::sin(t),z2=y*std::sin(t)+p[2]*std::cos(t);
    return {value("lx")+x*std::cos(psi)-z2*std::sin(psi),value("ly")+y2,value("lz")+x*std::sin(psi)+z2*std::cos(psi)};
}
void update_click_projection() {
    OANS_TRACE_SCOPE();
    click_update_at=XPLMGetElapsedTime();
    click_inverse[0].reset();click_inverse[1].reset();
    click_camera_gap=-1;
    click_world_used={};click_acf_used={};click_projection_used={};
    click_window_size[0]=click_window_size[1]=0;
    click_screen_bounds[0]=click_screen_bounds[1]=click_screen_bounds[2]=click_screen_bounds[3]=0;
    if(value("view")!=1026||value("vr")!=0||!dr("acf").has(15)||!dr("projection").has(15))return;
    double m[16],a[16],p[16];
    for(int i=0;i<16;++i) {m[i]=value("world",i);a[i]=value("acf",i);p[i]=value("projection",i);}
    // Audited ND corners are already relative to the nominal aircraft CG.
    // X-Plane composes acf_matrix in double precision before exposing floats.
    // Going via world_matrix loses millimetres at distant local origins, which
    // become several screen pixels for a cockpit display close to the eye.
    // Read the aircraft and projection transforms together in this window draw.
    if(dr("hx").numeric()&&dr("hy").numeric()&&dr("hz").numeric()&&dr("hpsi").numeric()&&dr("htheta").numeric()) {
        double head[3]={value("hx"),value("hy"),value("hz")};auto eye=rotate_point(head);
        double camera[3]={-(m[0]*m[12]+m[1]*m[13]+m[2]*m[14]),-(m[4]*m[12]+m[5]*m[13]+m[6]*m[14]),-(m[8]*m[12]+m[9]*m[13]+m[10]*m[14])};
        click_camera_gap=std::sqrt(std::pow(camera[0]-eye[0],2)+std::pow(camera[1]-eye[1],2)+std::pow(camera[2]-eye[2],2));
    }
    double ww=value("ww"),wh=value("wh");int l,t,r,b;XPLMGetScreenBoundsGlobal(&l,&t,&r,&b);
    click_window_size[0]=ww;click_window_size[1]=wh;
    click_screen_bounds[0]=l;click_screen_bounds[1]=t;click_screen_bounds[2]=r;click_screen_bounds[3]=b;
    for(int i=0;i<16;++i) {click_world_used[i]=m[i];click_acf_used[i]=a[i];click_projection_used[i]=p[i];}
    for(double v:a)if(!std::isfinite(v))return;
    if(std::abs(a[15]-1)>1e-6)return;
    if(ww<100||wh<100||r<=l||t<=b||std::abs(p[15])>1e-6)return;
    // Require a single main viewport. Multi-monitor and VR need their own camera mapping.
    double sx=(r-l)/ww,sy=(t-b)/wh;if(std::abs(sx-sy)>.03)return;
    for(int side=0;side<2;++side)if(nd_overlay_available(side)) {
        std::array<Vec2,4> q;bool valid=true;
        for(int k=0;k<4;++k) {
            const auto& v=profile::current.nd_corners[side][k];double e[4]={},c[4]={};
            for(int row=0;row<4;++row) {e[row]=a[12+row];for(int j=0;j<3;++j)e[row]+=a[j*4+row]*v[j];}
            for(int row=0;row<4;++row)for(int j=0;j<4;++j)c[row]+=p[j*4+row]*e[j];
            if(c[3]<.001) {valid=false;break;}
            q[k]={l+(c[0]/c[3]*.5+.5)*(r-l),b+(c[1]/c[3]*.5+.5)*(t-b)};
        }
        if(valid) {click_quad[side]=q;auto hom=Homography::quad(q);if(hom)click_inverse[side]=hom->inverse();}
    }
}
void overlay_draw(XPLMWindowID,void*) {
    OANS_TRACE_SCOPE();update_click_projection();}
bool nd_point(int id,int x,int y,Vec2& out,bool outside=false) {
    if(!click_inverse[id]||!nd_overlay_available(id))return false;
    auto p=click_inverse[id]->map({static_cast<double>(x),static_cast<double>(y)});
    if(!std::isfinite(p.x)||!std::isfinite(p.y))return false;
    out={p.x*600,(1-p.y)*600};
    return outside||(p.x>=0&&p.x<=1&&p.y>=0&&p.y<=1);
}
void pointer(int id,Vec2 p,Vec2 pixels,XPLMMouseStatus action,bool allow_actions=true) {
    OANS_TRACE_SCOPE();
    auto& s=views[id];auto& g=gestures[id];
    if(id<2) {
        if(allow_actions&&action==xplm_MouseDown&&oans_box.contains(p)) {
            g.begin(p,pixels,false);captured_view=id;return;
        }
        if(allow_actions&&g.pressed&&oans_box.contains(g.down)) {
            g.move(pixels);
            if(action==xplm_MouseUp) {
                const bool toggle=!g.moved&&oans_box.contains(p);
                release_input();
                if(toggle) {toggle_nd(id);s.menu=false;status=nd_on[id]?"OANS MAP ON":"OANS MAP OFF";}
            }
            return;
        }
        if(!nd_available(id))return;
    }
    if(action==xplm_MouseDown) {
        auto frame=nd_controls(s,ui_model());
        const auto* hit=frame.hit(p);
        if(p.y<status_bar_height()&&(!allow_actions||!hit||hit->action!=UiAction::panel_toggle))return;
        const bool map_area=p.y>=transform(id).bottom&&p.y<550&&(!s.menu||!allow_actions)&&!frame.hit(p)
            &&!(id<2&&oans_box.contains(p))&&(!allow_actions||map_label(id,p).first==UiAction::none);
        g.begin(p,pixels,map_area);captured_view=id;return;
    }
    if(!g.pressed)return;
    if(g.move(pixels)) {
        auto m=transform(id);s.center=m.center+(m.from(g.last)-m.from(p));s.follow=false;g.last=p;
    }
    if(action!=xplm_MouseUp)return;
    const bool moved=g.moved;const Vec2 down=g.down;g.clear();captured_view=-1;
    if(moved||!allow_actions)return;
    auto frame=nd_controls(s,ui_model());const auto* before=frame.hit(down);const auto* after=frame.hit(p);
    if(before) {
        if(after&&before->action==after->action&&before->argument==after->argument&&after->enabled)ui_action(id,after->action,after->argument);
        return;
    }
    if(s.menu) {s.menu=false;return;}
    if(s.panel!=Panel::closed)return; // panel background never selects a map label underneath
    if(p.y>=550) {ui_action(id,UiAction::orient);return;}
    if(p.y<55)return;
    const auto label=map_label(id,p);
    if(label.first==UiAction::runway_next) {
        select_runway(label.second);s.feature_kind=0;s.feature_selection=label.second;
    }
    else {if(input_owner>=0)release_input();show_context(s,p,transform(id).from(p));}
}
int overlay_click(XPLMWindowID,int x,int y,XPLMMouseStatus action,void*) {
    OANS_TRACE_SCOPE();OANS_TRACE("Mouse x="+std::to_string(x)+" y="+std::to_string(y)+" action="+std::to_string(action));
    if(captured_view>=0&&captured_view<2&&action!=xplm_MouseDown) {
        Vec2 p;const int id=captured_view;
        if(nd_point(id,x,y,p,true))pointer(id,p,{double(x),double(y)},action);
        else release_input();
        return 1;
    }
    for(int side=0;side<2;++side) {Vec2 p;if(!nd_point(side,x,y,p))continue;
        if(!nd_on[side]&&!oans_box.contains(p))continue;
        if(config.test&&!oans_box.contains(p)) {
            if(action==xplm_MouseDown) {++hit_count[side];std::ostringstream s;s<<"ND "<<side<<" click normalized="<<p.x/600<<','<<1-p.y/600;status=s.str();log(status);}
        } else {
            if(action==xplm_MouseDown)++hit_count[side];
            pointer(side,p,{double(x),double(y)},action);
        }
        return 1;
    }
    if(action==xplm_MouseDown&&input_owner>=0&&input_owner<2)release_input();
    return 0;
}
int overlay_right(XPLMWindowID,int x,int y,XPLMMouseStatus action,void*) {
    OANS_TRACE_SCOPE();OANS_TRACE("Mouse x="+std::to_string(x)+" y="+std::to_string(y)+" action="+std::to_string(action));
    if(captured_view>=0&&captured_view<2&&action!=xplm_MouseDown) {
        Vec2 p;const int id=captured_view;
        if(nd_point(id,x,y,p,true))pointer(id,p,{double(x),double(y)},action,false);
        else if(action==xplm_MouseUp) {gestures[id].clear();captured_view=-1;}
        return 1;
    }
    for(int id=0;id<2;++id) {Vec2 p;if(!nd_available(id)||!nd_point(id,x,y,p))continue;
        if(!config.test)pointer(id,p,{double(x),double(y)},action,false);
        return 1;
    }
    return 0;
}
void ui_key(XPLMWindowID w,char key,XPLMKeyFlags flags,char,void*,int losing) {
    OANS_TRACE_SCOPE();
    if(losing) {if(input_owner>=0)views[input_owner].editing=false;input_owner=-1;return;}
    if(!enabled) {if(input_owner>=0)release_input();return;}
    const int id=input_owner;
    if(id<0||!views[id].editing||views[id].panel!=Panel::airport||!(flags&xplm_DownFlag))return;
    if((id==2&&w!=window)||(id<2&&w!=mouse_window))return;
    if(key==13)display_airport(id);
    else if(key==8)ui_action(id,UiAction::backspace);
    else if(key==27) {release_input();views[id].panel=Panel::closed;}
    else if(std::isalnum(static_cast<unsigned char>(key))||key==' ')ui_action(id,UiAction::character,key);
}
XPLMCursorStatus cursor_noop(XPLMWindowID,int,int,void*) {return xplm_CursorDefault;}
int overlay_wheel(XPLMWindowID,int x,int y,int wheel,int clicks,void*) {
    OANS_TRACE_SCOPE();
    if(wheel!=0)return 0;
    for(int id=0;id<2;++id) {Vec2 p;if(!nd_available(id)||!nd_point(id,x,y,p)||oans_box.contains(p))continue;
        if(p.y<status_bar_height())return 1;
        p.y-=status_bar_height();
        if(!config.test&&!scroll_nd_dropdown(views[id],ui_model(),p,clicks))
            for(int i=0;i<std::min(10,std::abs(clicks));++i)zoom(views[id],clicks>0);
        return 1;
    }
    return 0;
}
void popup_draw(XPLMWindowID w,void*) {
    OANS_TRACE_SCOPE();
    OANS_TRACE("OpenGL capture/setup begin");
    DrawingState drawing_state(graphics,restore_graphics);
    OANS_TRACE("OpenGL capture/setup done");
    int l,t,r,b;XPLMGetWindowGeometry(w,&l,&t,&r,&b);const float bg[]={.035f,.05f,.065f};rect(l,b,r,t,bg);
    text(l+12,t-24,std::string("ToLiss OANS ")+version+" - EXPERIMENTAL",cyan);
    if(!enabled) {text(l+12,t-55,"PLUGIN DISABLED",amber);return;}
    text(l+12,t-68,"ND CONTROLS: MAP DATA / ARPT SEL / STATUS | LEFT CLICK MENU / LEFT OR RIGHT DRAG PAN");
    double size=std::min(r-l-24,t-b-150);if(size>50)render_rect(l+12,b+62,size,size,2,false);
    text(l+12,b+42,status.substr(0,100),amber);
    text(l+12,b+22,status.substr(0,100));
    text(l+12,b+6,"Airport input is inside ARPT SEL. Same controls on both NDs.",cyan);
}
bool popup_point(XPLMWindowID w,int x,int y,Vec2& p,bool outside=false) {
    int l,t,r,b;XPLMGetWindowGeometry(w,&l,&t,&r,&b);
    double size=std::min(r-l-24,t-b-150);
    if(size<=50)return false;
    p={(x-l-12)*600/size,(y-b-62)*600/size};
    return outside||(p.x>=0&&p.x<=600&&p.y>=0&&p.y<=600);
}
std::optional<Vec2> pointer_on_map(int id) {
    if(cursor_view!=id||XPLMGetElapsedTime()-cursor_seen>.25)return {};
    Vec2 p;
    if(id==2) {if(!XPLMGetWindowIsVisible(window)||!popup_point(window,cursor_x,cursor_y,p))return {};}
    else if(!nd_available(id)||!nd_point(id,cursor_x,cursor_y,p))return {};
    if(p.y<status_bar_height())return {};
    return p;
}
XPLMCursorStatus map_cursor(XPLMWindowID w,int x,int y,void*) {
    OANS_TRACE_SCOPE();
    cursor_view=-1;
    if(!enabled)return xplm_CursorDefault;
    Vec2 p;
    if(w==window) {if(popup_point(w,x,y,p)&&p.y>=status_bar_height())cursor_view=2;}
    else for(int side=0;side<2;++side)if(nd_available(side)&&nd_point(side,x,y,p)&&p.y>=status_bar_height()) {cursor_view=side;break;}
    if(cursor_view<0)return xplm_CursorDefault;
    cursor_x=x;cursor_y=y;cursor_seen=XPLMGetElapsedTime();return xplm_CursorHidden;
}
int popup_click(XPLMWindowID w,int x,int y,XPLMMouseStatus action,void*) {
    OANS_TRACE_SCOPE();OANS_TRACE("Mouse x="+std::to_string(x)+" y="+std::to_string(y)+" action="+std::to_string(action));
    if(!enabled)return 1;
    Vec2 p;
    if(popup_point(w,x,y,p,captured_view==2))pointer(2,p,{double(x),double(y)},action);
    else if(action==xplm_MouseDown&&input_owner==2)release_input();
    return 1;
}
int popup_right(XPLMWindowID w,int x,int y,XPLMMouseStatus action,void*) {
    OANS_TRACE_SCOPE();OANS_TRACE("Mouse x="+std::to_string(x)+" y="+std::to_string(y)+" action="+std::to_string(action));
    if(!enabled)return 1;
    if(captured_view==2&&action!=xplm_MouseDown) {
        Vec2 p;
        if(popup_point(w,x,y,p,true))pointer(2,p,{double(x),double(y)},action,false);
        else if(action==xplm_MouseUp) {gestures[2].clear();captured_view=-1;}
        return 1;
    }
    Vec2 p;if(popup_point(w,x,y,p))pointer(2,p,{double(x),double(y)},action,false);return 1;
}
int popup_wheel(XPLMWindowID w,int x,int y,int wheel,int clicks,void*) {
    OANS_TRACE_SCOPE();
    if(!enabled)return 1;
    Vec2 p;if(wheel==0&&popup_point(w,x,y,p)&&p.y>=status_bar_height()&&
        !scroll_nd_dropdown(views[2],ui_model(),{p.x,p.y-status_bar_height()},clicks))
        for(int i=0;i<std::min(10,std::abs(clicks));++i)zoom(views[2],clicks>0);
    return 1;
}
void stop_advisory_test() {
    advisory_test.active=advisory_test.pending=false;advisory_test.airport.reset();advisory_test.hits.clear();
    advisory_test.message="Test stopped. Live runway detection restored.";
}
void finish_advisory_test() {
    if(!advisory_test.pending||!airport||airport->icao!=advisory_test.requested_airport)return;
    try {
        advisory_test.hits=test_runways(*airport,advisory_test.requested_runways);
        advisory_test.airport=airport;advisory_test.active=true;advisory_test.pending=false;
        const auto& id=advisory_test.hits.front().first;
        const auto found=std::find_if(airport->runways.begin(),airport->runways.end(),[&](const auto& r){return r.name==id;});
        for(auto& view:views) {view.follow=false;view.center=(found->start+found->end)*.5;view.range_nm=1;}
        nd_on[0]=true;
        advisory_test.message="ACTIVE: "+airport->icao+" "+advisory_test.requested_runways+" / "+(advisory_test.steady?"STEADY":"BLINK");
        log("RWY AHEAD TEST enabled: "+airport->icao+" "+advisory_test.requested_runways);
    } catch(const std::exception& e) {stop_advisory_test();advisory_test.message=e.what();}
}
void start_advisory_test() {
    stop_advisory_test();
    if(!enabled) {advisory_test.message="PLUGIN DISABLED";return;}
    if(!index_ready) {advisory_test.message="Airport index is loading. Try again shortly.";return;}
    auto query=advisory_test.airport_query;
    query.erase(std::remove_if(query.begin(),query.end(),[](unsigned char c){return std::isspace(c);}),query.end());
    for(char& c:query)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    std::optional<IndexEntry> entry;
    {std::lock_guard<std::mutex> lock(db_mutex);entry=find_airport(airport_index,query);}
    if(!entry) {advisory_test.message="AIRPORT NOT FOUND: "+query;return;}
    if(advisory_test.runway_query.empty()) {advisory_test.message="ENTER A RUNWAY END, E.G. 02L";return;}
    advisory_test.requested_airport=entry->icao;advisory_test.requested_runways=advisory_test.runway_query;
    advisory_test.pending=true;advisory_test.message="Loading "+entry->icao+"...";
    manual_airport=true;
    if(airport&&airport->icao==entry->icao&&a350_map)finish_advisory_test();
    else request_airport(*entry);
}
void close_test_window() {
    if(!test_window)return;
    if(advisory_test.focused)XPLMTakeKeyboardFocus(nullptr);
    advisory_test.focused=false;XPLMSetWindowIsVisible(test_window,0);
}
void focus_test_field(int field) {
    release_input();advisory_test.field=field;advisory_test.replace=true;
    XPLMTakeKeyboardFocus(test_window);advisory_test.focused=true;
}
void toggle_test_window() {
    if(!test_window) {status="TEST WINDOW UNAVAILABLE";return;}
    if(XPLMGetWindowIsVisible(test_window)) {close_test_window();return;}
    if(advisory_test.airport_query.empty()&&airport)advisory_test.airport_query=airport->icao;
    if(advisory_test.runway_query.empty()&&airport&&!airport->runways.empty())advisory_test.runway_query=airport->runways.front().name;
    XPLMSetWindowIsVisible(test_window,1);focus_test_field(0);
}
const Box test_airport_box{140,232,532,266},test_runway_box{140,180,532,214};
const Box test_steady_box{20,132,330,164},test_enable_box{20,82,190,116},test_stop_box{204,82,362,116},test_close_box{376,82,532,116};
void test_draw(XPLMWindowID w,void*) {
    OANS_TRACE_SCOPE();
    OANS_TRACE("OpenGL capture/setup begin");
    DrawingState drawing_state(graphics,restore_graphics);
    OANS_TRACE("OpenGL capture/setup done");int l,t,r,b;XPLMGetWindowGeometry(w,&l,&t,&r,&b);
    const float bg[]={.035f,.05f,.065f},fill[]={.08f,.1f,.12f};rect(l,b,r,t,bg);
    auto button=[&](const Box& box,const std::string& label,bool selected=false) {
        const float* edge=selected?cyan:white;rect(l+box.l,b+box.b,l+box.r,b+box.t,edge);
        rect(l+box.l+1,b+box.b+1,l+box.r-1,b+box.t-1,fill);text(l+box.l+8,b+box.b+11,label,selected?cyan:white);
    };
    text(l+20,t-30,"RWY AHEAD TEST",cyan);
    text(l+20,b+244,"Airport");text(l+20,b+192,"Runway(s)");
    button(test_airport_box,advisory_test.airport_query+(advisory_test.focused&&advisory_test.field==0?" |":""),advisory_test.focused&&advisory_test.field==0);
    button(test_runway_box,advisory_test.runway_query+(advisory_test.focused&&advisory_test.field==1?" |":""),advisory_test.focused&&advisory_test.field==1);
    button(test_steady_box,advisory_test.steady?"[x] Steady display (screenshot)":"[ ] Steady display (blink enabled)",advisory_test.steady);
    button(test_enable_box,"Enable test",advisory_test.active);button(test_stop_box,"Stop test");button(test_close_box,"Close");
    text(l+20,b+57,advisory_test.message.substr(0,66),advisory_test.active?green:amber);
    text(l+20,b+34,"Example: ZUCK / 02L or 02L,02R. TAB switches fields.");
    text(l+20,b+14,"Close keeps test active. Stop test returns to live warnings.",cyan);
}
void test_key(XPLMWindowID w,char key,XPLMKeyFlags flags,char virtual_key,void*,int losing) {
    OANS_TRACE_SCOPE();
    if(losing) {advisory_test.focused=false;return;}
    if(w!=test_window||!enabled||!XPLMGetWindowIsVisible(w)||!(flags&xplm_DownFlag))return;
    if(key==27) {close_test_window();return;}
    if(key==9) {focus_test_field(1-advisory_test.field);return;}
    if(key==13) {start_advisory_test();return;}
    auto& field=advisory_test.field==0?advisory_test.airport_query:advisory_test.runway_query;
    if(flags&xplm_ControlFlag) {
        if(virtual_key==XPLM_VK_A||key=='a'||key=='A') {advisory_test.replace=true;return;}
        if(virtual_key==XPLM_VK_V&&OpenClipboard(nullptr)) {
            if(auto data=GetClipboardData(CF_UNICODETEXT))if(auto ptr=static_cast<const wchar_t*>(GlobalLock(data))) {
                if(advisory_test.replace)field.clear();
                for(size_t i=0;ptr[i]&&field.size()<(advisory_test.field==0?8u:32u);++i) {
                    wchar_t c=ptr[i];if(c<128&&(std::isalnum(static_cast<unsigned char>(c))||c==' '||c==','||c=='/'||c=='-'))field+=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                }
                GlobalUnlock(data);advisory_test.replace=false;
            }
            CloseClipboard();
        }
        return;
    }
    if(key==8) {if(advisory_test.replace)field.clear();else if(!field.empty())field.pop_back();advisory_test.replace=false;return;}
    if(std::isalnum(static_cast<unsigned char>(key))||key==' '||key==','||key=='/'||key=='-') {
        if(advisory_test.replace)field.clear();
        advisory_test.replace=false;
        if(field.size()<(advisory_test.field==0?8u:32u))field+=static_cast<char>(std::toupper(static_cast<unsigned char>(key)));
    }
}
int test_click(XPLMWindowID w,int x,int y,XPLMMouseStatus action,void*) {
    OANS_TRACE_SCOPE();OANS_TRACE("Mouse x="+std::to_string(x)+" y="+std::to_string(y)+" action="+std::to_string(action));
    if(action!=xplm_MouseDown||!enabled)return 1;
    int l,t,r,b;XPLMGetWindowGeometry(w,&l,&t,&r,&b);Vec2 p{double(x-l),double(y-b)};
    if(test_airport_box.contains(p))focus_test_field(0);
    else if(test_runway_box.contains(p))focus_test_field(1);
    else if(test_steady_box.contains(p)) {
        advisory_test.steady=!advisory_test.steady;
        if(advisory_test.active)advisory_test.message="ACTIVE: "+advisory_test.requested_airport+" "+advisory_test.requested_runways+" / "+(advisory_test.steady?"STEADY":"BLINK");
    }
    else if(test_enable_box.contains(p))start_advisory_test();
    else if(test_stop_box.contains(p))stop_advisory_test();
    else if(test_close_box.contains(p))close_test_window();
    return 1;
}
int test_wheel(XPLMWindowID,int,int,int,int,void*) {return 1;}
void toggle_window() {if(window) {if(input_owner==2)release_input();
    const bool visible=!XPLMGetWindowIsVisible(window);
    if(visible)confirm_first_oans_position();
    XPLMSetWindowIsVisible(window,visible);}}
void menu_action(void*,void* item) {
    OANS_TRACE_SCOPE();
    const auto action=reinterpret_cast<std::intptr_t>(item);
    OANS_TRACE("Menu action="+std::to_string(action));
    if(!enabled&&action!=1&&action!=5)return;
    switch(action) {
        case 1:toggle_window();break;
        case 2:release_input();toggle_nd(0);break;
        case 3:release_input();toggle_nd(1);break;
        case 4:release_input();config.test=!config.test;log(config.test?"ND test pattern ON":"ND test pattern OFF");break;
        case 5:diagnostics();break;
        case 6:release_input();status="CONFIG RELOADED";fms_port.clear(true);fms_bind_at=0;fms_generation_seen=0;read_config();bind_refs();inspect_model();diagnostics();break;
        case 8:nearest_airport();break;
        case 9:open_panel(0,Panel::map_data);break;
        case 10:open_panel(1,Panel::map_data);break;
        case 11:open_panel(0,Panel::airport);break;
        case 12:open_panel(1,Panel::airport);break;
        case 13:zoom(views[0],true);break;
        case 14:zoom(views[0],false);break;
        case 15:zoom(views[1],true);break;
        case 16:zoom(views[1],false);break;
        case 17:views[0].follow=true;break;
        case 18:views[1].follow=true;break;
        case 19:toggle_test_window();break;
        case 20:case 21:case 22: {
            release_input();
            const bool bar=action==22;
            if(bar) {
                config.status_bar=!config.status_bar;
                XPLMCheckMenuItem(settings_menu,status_bar_item,config.status_bar?xplm_Menu_Checked:xplm_Menu_Unchecked);
            } else {config.simple_stand_names=action==20;update_stand_names();}
            current_surface.clear();
            surface_sample_at=0;
            surface_tracker.reset();
            const std::string setting=bar?"stus_bar":"stand_name";
            std::ifstream in(plugin_dir/"config.ini");std::string line,contents;
            while(std::getline(in,line)) {
                auto key=line.substr(0,line.find('='));
                key.erase(std::remove_if(key.begin(),key.end(),[](unsigned char c){return std::isspace(c);}),key.end());
                if(key!=setting)contents+=line+"\n";
            }
            in.close();
            std::ofstream out(plugin_dir/"config.ini");
            out<<contents<<setting<<'='<<(bar?(config.status_bar?"1":"0"):(config.simple_stand_names?"simple":"complex"))<<"\n";
            if(!out)log("Could not save "+setting+" setting");
            break;
        }
    }
}
int command(XPLMCommandRef,XPLMCommandPhase phase,void* data) {
    OANS_TRACE_SCOPE();if(phase==xplm_CommandBegin)menu_action(nullptr,data);return 1;}
void create_ui() {
    OANS_TRACE_SCOPE();
    int left,top,right,bottom;XPLMGetScreenBoundsGlobal(&left,&top,&right,&bottom);
    XPLMCreateWindow_t w{};w.structSize=sizeof w;w.left=left+80;w.top=std::min(top-40,bottom+860);
    w.right=w.left+660;w.bottom=std::max(bottom+20,w.top-780);w.visible=0;
    w.drawWindowFunc=popup_draw;w.handleMouseClickFunc=popup_click;w.handleKeyFunc=ui_key;
    w.handleCursorFunc=map_cursor;w.handleMouseWheelFunc=popup_wheel;w.handleRightClickFunc=popup_right;
    w.decorateAsFloatingWindow=xplm_WindowDecorationRoundRectangle;w.layer=xplm_WindowLayerFloatingWindows;
    window=XPLMCreateWindowEx(&w);if(window) {XPLMSetWindowTitle(window,"ToLiss OANS");XPLMSetWindowResizingLimits(window,620,700,1100,1200);}
    w.visible=1;w.decorateAsFloatingWindow=xplm_WindowDecorationNone;w.layer=xplm_WindowLayerFloatingWindows;
    w.drawWindowFunc=overlay_draw;w.handleMouseClickFunc=overlay_click;w.handleRightClickFunc=overlay_right;w.handleKeyFunc=ui_key;w.handleMouseWheelFunc=overlay_wheel;
    mouse_window=XPLMCreateWindowEx(&w);
    w.left=left+180;w.top=std::min(top-80,bottom+700);w.right=w.left+560;w.bottom=w.top-340;w.visible=0;
    w.decorateAsFloatingWindow=xplm_WindowDecorationRoundRectangle;
    w.drawWindowFunc=test_draw;w.handleMouseClickFunc=test_click;w.handleRightClickFunc=test_click;w.handleKeyFunc=test_key;w.handleMouseWheelFunc=test_wheel;
    w.handleCursorFunc=cursor_noop;
    test_window=XPLMCreateWindowEx(&w);
    if(test_window) {XPLMSetWindowTitle(test_window,"ToLiss OANS - Test");XPLMSetWindowResizingLimits(test_window,560,340,560,340);}
    menu_item=XPLMAppendMenuItem(XPLMFindPluginsMenu(),"ToLiss OANS",nullptr,0);
    menu=XPLMCreateMenu("ToLiss OANS",XPLMFindPluginsMenu(),menu_item,menu_action,nullptr);
    const char* labels[]={"Open Airport Map Window","Toggle captain ND map","Toggle FO ND map","Toggle ND calibration pattern","Write interface / click report","Reload config and aircraft profile","","Return to nearest airport",
        "Captain ND MAP DATA","FO ND MAP DATA","Captain ND ARPT SEL","FO ND ARPT SEL","Captain OANS zoom in","Captain OANS zoom out","FO OANS zoom in","FO OANS zoom out","Captain center on aircraft","FO center on aircraft"};
    const char* paths[]={"toliss_oans/window","toliss_oans/capt_nd","toliss_oans/fo_nd","toliss_oans/test_pattern","toliss_oans/diagnostics","toliss_oans/reload","","toliss_oans/nearest",
        "toliss_oans/capt_panel","toliss_oans/fo_panel","toliss_oans/capt_airport","toliss_oans/fo_airport","toliss_oans/capt_range_in","toliss_oans/capt_range_out","toliss_oans/fo_range_in","toliss_oans/fo_range_out","toliss_oans/capt_center","toliss_oans/fo_center"};
    for(int i=0;i<18;++i) {
        if(i==6)continue; // Retired command slot; preserve existing OANS action IDs.
        auto token=reinterpret_cast<void*>(static_cast<std::intptr_t>(i+1));
        if(i==0)XPLMAppendMenuItem(menu,labels[i],token,0);
        auto c=XPLMCreateCommand(paths[i],labels[i]);
        XPLMRegisterCommandHandler(c,command,1,token);commands.push_back({c,i+1});
    }
    auto test_token=reinterpret_cast<void*>(static_cast<std::intptr_t>(19));
    XPLMAppendMenuItem(menu,"Test",test_token,0);
    auto test_command=XPLMCreateCommand("toliss_oans/test","Open runway advisory test window");
    XPLMRegisterCommandHandler(test_command,command,1,test_token);commands.push_back({test_command,19});
    const int setting_item=XPLMAppendMenuItem(menu,"Setting",nullptr,0);
    settings_menu=XPLMCreateMenu("Setting",menu,setting_item,menu_action,nullptr);
    const int stand_item=XPLMAppendMenuItem(settings_menu,"Stand name",nullptr,0);
    stand_name_menu=XPLMCreateMenu("Stand name",settings_menu,stand_item,menu_action,nullptr);
    XPLMAppendMenuItem(stand_name_menu,"Simple",reinterpret_cast<void*>(std::intptr_t(20)),0);
    XPLMAppendMenuItem(stand_name_menu,"Complex",reinterpret_cast<void*>(std::intptr_t(21)),0);
    status_bar_item=XPLMAppendMenuItem(settings_menu,"stus_bar",reinterpret_cast<void*>(std::intptr_t(22)),0);
    XPLMCheckMenuItem(settings_menu,status_bar_item,config.status_bar?xplm_Menu_Checked:xplm_Menu_Unchecked);
    update_stand_names();
}
float tick(float elapsed,float,int,void*) {
    OANS_TRACE_SCOPE();
    try {
        heartbeat=XPLMGetElapsedTime();
        if(!enabled) {traffic_targets.clear();return .1f;}
        if(heartbeat>=bind_at) {bind_at=heartbeat+5;bind_refs();}
        if(heartbeat>=traffic_sample_at) {traffic_sample_at=heartbeat+.1;refresh_tcas_targets();}
        if(!aircraft_ok&&heartbeat>=model_check_at) {model_check_at=heartbeat+5;inspect_model();}
        if(config.fms&&aircraft_ok&&heartbeat>=fms_bind_at) {fms_bind_at=heartbeat+5;fms_port.bind();}
        fms_port.tick(heartbeat,config.fms&&aircraft_ok&&std::max(value("bus",0),value("bus",1))>=20);
        if(fms_generation_seen!=fms_port.sampler.generation()) {
            fms_generation_seen=fms_port.sampler.generation();
            choose_at=0;
        }
        const bool position_ready=aircraft_position_ready();
        if(!position_ready)traffic_targets.clear();
        if(!position_ready&&!manual_airport&&!airport_position_pending)nearest_airport();
        if(position_ready&&index_ready&&airport_position_pending&&!manual_airport) {
            std::optional<IndexEntry> nearby;
            {std::lock_guard<std::mutex> lock(db_mutex);nearby=nearest(airport_index,value("lat"),value("lon"));}
            airport_position_pending=false;choose_at=heartbeat+5;
            if(nearby) {request_airport(*nearby);status="LOADING "+nearby->icao;}
            else {airport_map_loading=false;status="ARPT NAV NOT AVAIL";}
            log("OANS position confirmed: "+(nearby?nearby->icao:"NO NEARBY AIRPORT"));
        }
        if((input_owner>=0&&input_owner<2&&!nd_available(input_owner))||
           (input_owner==2&&!XPLMGetWindowIsVisible(window)))release_input();
        if(captured_view>=0&&captured_view<2&&!nd_overlay_available(captured_view))release_input();
        if(advisory_test.focused&&test_window&&!XPLMGetWindowIsVisible(test_window)) {
            XPLMTakeKeyboardFocus(nullptr);advisory_test.focused=false;
        }
        if(index_ready)for(int id=0;id<3;++id)if(views[id].panel==Panel::airport&&!search_built[id])refresh_search(id);
        {
            std::lock_guard<std::mutex> lock(db_mutex);
            if(pending_advisory) {
                if(pending_advisory->icao==advisory_requested)advisory_airport=std::move(pending_advisory);
                pending_advisory.reset();
            }
            if(index_ready&&heartbeat>=advisory_choose_at) {
                advisory_choose_at=heartbeat+5;
                auto nearby=nearest(airport_index,value("lat"),value("lon"));
                if(!nearby) {advisory_airport.reset();advisory_requested.clear();advisory_request.reset();}
                else if(nearby->icao!=advisory_requested||(!advisory_airport&&!advisory_request)) {
                    advisory_requested=nearby->icao;advisory_request=*nearby;advisory_airport.reset();db_cv.notify_one();
                }
            }
        }
        RunwayAheadInput advisory_input;
        if(advisory_airport)advisory_input.position=local(value("lat"),value("lon"),advisory_airport->lat,advisory_airport->lon);
        advisory_input.speed=value("speed");advisory_input.heading=value("psi");
        advisory_input.ground=value("ground",1)>0||value("ground",2)>0;advisory_input.paused=value("paused")!=0;
        advisory_input.valid=aircraft_ok&&profile_ok&&dr("lat").numeric()&&dr("lon").numeric()&&dr("speed").numeric()&&
            dr("psi").numeric()&&dr("ground").numeric(2)&&std::isfinite(value("lat"))&&std::isfinite(value("lon"));
        runway_advisory.update(advisory_airport.get(),advisory_input,elapsed);
        std::shared_ptr<Airport> loaded;std::shared_ptr<A350Map> loaded_map;std::string err,map_note,map_icao;
        {std::lock_guard<std::mutex> lock(db_mutex);loaded=std::move(pending_airport);loaded_map=std::move(pending_map);
            map_note=std::move(pending_map_note);pending_map_note.clear();
            map_icao=std::move(pending_map_icao);pending_map_icao.clear();
            pending_map_is_building_refresh=false;
            err=std::move(worker_error);worker_error.clear();}
        if(!err.empty()) {status="AIRPORT DATA ERROR: "+err;log(status);}
        if(advisory_test.pending&&(!err.empty()||last_requested!=advisory_test.requested_airport)) {
            stop_advisory_test();advisory_test.message=err.empty()?"Airport selection changed. Enable test again.":"AIRPORT LOAD FAILED: "+err;
        }
        if(loaded&&loaded->icao!=last_requested) {log("Discarded superseded airport load: "+loaded->icao);loaded.reset();}
        if(loaded) {
            airport_map_loading=false;
            status="AIRPORT LOADED";airport=std::move(loaded);a350_map=std::move(loaded_map);runway_index=-1;
            if(!map_note.empty())log(map_note);
            for(auto& s:views) {
                s.follow=!manual_airport;
                s.center=local(airport->reference.lat,airport->reference.lon,airport->lat,airport->lon);
                s.feature_selection=-1;s.crosses.clear();s.flags.clear();
            }
            for(auto& items:map_features)items.clear();
            for(const auto& r:airport->runways)map_features[0].push_back({r.name,r.start+r.axis()*r.displaced});
            std::map<std::string,std::pair<Vec2,int>> taxi_points;
            for(const auto& e:airport->edges)if(!e.runway&&!e.name.empty()) {
                auto a=airport->nodes.find(e.a),b=airport->nodes.find(e.b);
                if(a!=airport->nodes.end()&&b!=airport->nodes.end()) {auto& item=taxi_points[e.name];item.first=item.first+(a->second+b->second)*.5;++item.second;}
            }
            for(const auto& item:taxi_points)map_features[1].push_back({item.first,item.second.first*(1.0/item.second.second)});
            update_stand_names();
            if(a350_map)if(const auto tower=a350_map->tower())
                map_features[3].push_back({"TWR",local(tower->lat,tower->lon,airport->lat,airport->lon)});
            std::sort(map_features[2].begin(),map_features[2].end(),[](const auto& a,const auto& b){return a.name<b.name;});
            for(auto& view:views) {view.feature_selection=-1;view.feature_dropdown=view.feature_initial_dropdown=false;view.feature_first=0;view.feature_initial='-';}
            status="LOADED "+airport->icao+" "+airport->name;log(status);select_runway(0);
            finish_advisory_test();
        } else if(loaded_map&&airport&&map_icao==airport->icao&&map_icao==last_requested) {
            a350_map=std::move(loaded_map);
            if(!map_note.empty())log(map_note);
        } else if(!map_note.empty()&&airport&&map_icao==airport->icao&&map_icao==last_requested) {
            log(map_note);
        }
        if(index_ready&&position_ready&&!airport_position_pending&&heartbeat>choose_at&&!manual_airport&&input_owner<0) {
            choose_at=heartbeat+5;
            std::optional<IndexEntry> target;
            {std::lock_guard<std::mutex> lock(db_mutex);
                auto nearby=nearest(airport_index,value("lat"),value("lon"));
                const auto& p=fms_port.sampler.plan();
                auto dest=p.observed?find_airport(airport_index,p.destination.airport):std::nullopt;
                const bool airborne=dr("ground").numeric(2)&&value("ground",0)==0&&value("ground",1)==0&&value("ground",2)==0;
                if(airborne&&dest) {
                    const bool near_dep=nearby&&nearby->icao!=dest->icao&&
                        length(local(value("lat"),value("lon"),nearby->lat,nearby->lon))<15000&&
                        (nearby->icao==p.origin.airport||(airport&&nearby->icao==airport->icao));
                    if(!near_dep||length(local(value("lat"),value("lon"),dest->lat,dest->lon))<40000)target=dest;
                }
                if(!target&&(!airport||length(local(value("lat"),value("lon"),airport->lat,airport->lon))>60000))target=nearby;
            }
            if(target&&target->icao!=last_requested)request_airport(*target);
        }
        if(!config.status_bar||!airport||airport_map_loading||!aircraft_position_ready()||
           !dr("ground").numeric(2)||(value("ground",0)<=0&&value("ground",1)<=0&&value("ground",2)<=0)) {
            current_surface.clear();surface_sample_at=0;surface_tracker.reset();
        } else if(heartbeat>=surface_sample_at) {
            surface_sample_at=heartbeat+.5;
            current_surface=surface_tracker.update(*airport,local(value("lat"),value("lon"),airport->lat,airport->lon),value("psi"),heartbeat,config.simple_stand_names);
        }
        if(mouse_window) {int l,tp,r,b;XPLMGetScreenBoundsGlobal(&l,&tp,&r,&b);XPLMSetWindowGeometry(mouse_window,l,tp,r,b);}
    } catch(const std::exception& e) {status="FLIGHT LOOP ERROR";log(e.what());}
    return -1;
}
void cleanup() {
    OANS_TRACE_SCOPE();
    enabled=false;fms_port.clear(true);release_input();finish_worker();
    traffic_targets.clear();
    a350_map.reset();pending_map.reset();a350_ui.reset();map_font.release();
    for(auto& texture:map_textures)texture.release();
    for(auto& owner:texture_maps)owner.reset();
    runway_advisory.reset();advisory_airport.reset();pending_advisory.reset();advisory_request.reset();advisory_requested.clear();advisory_choose_at=0;
    advisory_test=AdvisoryTest{};
    if(loop_registered)XPLMUnregisterFlightLoopCallback(tick,nullptr);
    loop_registered=false;
    if(draw_registered)XPLMUnregisterDrawCallback(gauge_draw,xplm_Phase_Gauges,0,nullptr);
    draw_registered=false;
    for(const auto& c:commands)XPLMUnregisterCommandHandler(c.first,command,1,reinterpret_cast<void*>(static_cast<std::intptr_t>(c.second)));
    commands.clear();
    if(mouse_window)XPLMDestroyWindow(mouse_window);
    mouse_window=nullptr;
    if(test_window)XPLMDestroyWindow(test_window);
    test_window=nullptr;
    if(window)XPLMDestroyWindow(window);
    window=nullptr;
    if(stand_name_menu)XPLMDestroyMenu(stand_name_menu);
    stand_name_menu=nullptr;
    if(settings_menu)XPLMDestroyMenu(settings_menu);
    settings_menu=nullptr;
    status_bar_item=-1;current_surface.clear();
    if(menu)XPLMDestroyMenu(menu);
    menu=nullptr;
    if(menu_item>=0)XPLMRemoveMenuItem(XPLMFindPluginsMenu(),menu_item);
    menu_item=-1;
}
}
PLUGIN_API int XPluginStart(char* name,char* signature,char* description) {
    // Resolve our own DLL before calling any XPLM interface, so startup faults
    // in SDK/provider calls can still leave a breadcrumb in detail.log.
    try {
        HMODULE module=nullptr;wchar_t binary[32768]={};
        if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&XPluginStart),&module)&&GetModuleFileNameW(module,binary,32768)) {
            plugin_dir=std::filesystem::path(binary).parent_path().parent_path();
            if(!detail::initialize(plugin_dir,version))XPLMDebugString("[ToLissOANS] Cannot open detail.log in plugin directory\n");
        }
    }catch(...) {XPLMDebugString("[ToLissOANS] Cannot initialize detail.log\n");}
    OANS_TRACE_SCOPE();OANS_TRACE(std::string("XPluginStart build=")+detail::flavor);
    std::strcpy(name,detail::debug?"ToLiss OANS (debug)":"ToLiss OANS (experimental)");std::strcpy(signature,"org.toliss-oans.experimental");
    std::strcpy(description,"Airport navigation display for the inspected ToLiss A320 1.3.2.");
    try {
        XPLMEnableFeature("XPLM_USE_NATIVE_PATHS",1);XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS",1);
        char plugin[2048]={},root[2048]={};XPLMGetPluginInfo(XPLMGetMyID(),nullptr,plugin,nullptr,nullptr);XPLMGetSystemPath(root);
        plugin_dir=std::filesystem::u8path(plugin).parent_path().parent_path();sim_dir=std::filesystem::u8path(root);
        int xp_version=0,sdk_version=0;XPLMHostApplicationID host{};
#if OANS_DEBUG
        OANS_TRACE("SDK XPLMGetVersions begin");XPLMGetVersions(&xp_version,&sdk_version,&host);
        OANS_TRACE("Environment X-Plane="+std::to_string(xp_version)+" SDK="+std::to_string(sdk_version)+" host="+std::to_string(host)+" pointer_bits="+std::to_string(sizeof(void*)*8)+" plugin="+plugin_dir.u8string()+" sim="+sim_dir.u8string());
#else
        (void)xp_version;(void)sdk_version;(void)host;
#endif
        std::ifstream cycle_info(sim_dir/"Custom Data/cycle_info.txt");
        airport_database_status=read_airport_database_status(cycle_info);
        OANS_TRACE("Startup font load begin");
        if(!map_font.load(plugin_dir/"assets/font_b612_px2.bin"))log("B612 font missing or invalid; SDK font fallback");
        OANS_TRACE("Startup A350 UI initialization begin");
        a350_ui=std::make_unique<A350Map>(plugin_dir/"assets");
        const auto& bounds=a350_ui->oans_switch(false).bounds;
        oans_box={bounds[0],bounds[1],bounds[2],bounds[3]};
        nd_on[0]=nd_on[1]=false;
        read_config();bind_refs(false);inspect_model();create_ui();
        start_worker();XPLMRegisterFlightLoopCallback(tick,-1,nullptr);loop_registered=true;
        OANS_TRACE("Register gauges draw callback begin");
        draw_registered=XPLMRegisterDrawCallback(gauge_draw,xplm_Phase_Gauges,0,nullptr)!=0;
        OANS_TRACE("Register gauges draw callback result="+std::to_string(draw_registered));
        // Keep the initial registration: 00_ToLissOANS must load before the
        // aircraft systems for this callback to retain the final ND image.
        if(!draw_registered)status="ND DRAW CALLBACK UNAVAILABLE: USE PLUGIN WINDOW";
        diagnostics();log("Native OANS plugin loaded. ND profile loaded="+std::to_string(profile_ok));return 1;
    } catch(const std::exception& e) {log(e.what());cleanup();detail::shutdown();return 0;}
}
PLUGIN_API void XPluginStop() {{OANS_TRACE_SCOPE();cleanup();OANS_TRACE("Plugin shutdown complete");}detail::shutdown();}
PLUGIN_API int XPluginEnable() {OANS_TRACE_SCOPE();enabled=true;return 1;}
PLUGIN_API void XPluginDisable() {OANS_TRACE_SCOPE();enabled=false;traffic_targets.clear();runway_advisory.reset();stop_advisory_test();if(test_window)close_test_window();fms_port.clear(true);release_input();status="PLUGIN DISABLED";}
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID from,int message,void* param) {
    OANS_TRACE_SCOPE();OANS_TRACE("SDK message from="+std::to_string(from)+" message="+std::to_string(message)+" param="+std::to_string(reinterpret_cast<std::intptr_t>(param)));
    (void)from;
    if(message==XPLM_MSG_AIRPORT_LOADED||
       ((message==XPLM_MSG_PLANE_LOADED||message==XPLM_MSG_PLANE_UNLOADED)&&reinterpret_cast<std::intptr_t>(param)==0)) {
        stop_advisory_test();if(test_window)close_test_window();
        nearest_airport();first_oans_open=true;
        runway_advisory.reset();advisory_airport.reset();advisory_choose_at=0;
        {std::lock_guard<std::mutex> lock(db_mutex);
            advisory_request.reset();pending_advisory.reset();advisory_requested.clear();}
        fms_port.clear(false);fms_bind_at=0;fms_generation_seen=0;
    }
    if(message==XPLM_MSG_PLANE_UNLOADED&&reinterpret_cast<std::intptr_t>(param)==0) {
        aircraft_ok=false;
    }
    if(message==XPLM_MSG_PLANE_LOADED&&reinterpret_cast<std::intptr_t>(param)==0) {
        status="AIRCRAFT RELOADED";bind_refs(false);aircraft_ok=false;bind_at=0;model_check_at=0;
    }
}
