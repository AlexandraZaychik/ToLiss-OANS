#include "a350_map.hpp"
#include "map_viewport.hpp"
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>

namespace {
size_t colour_count(const std::vector<oans::A350Vertex>& vertices,std::uint32_t colour) {
    size_t count=0;for(const auto& v:vertices)if(v.rgba==colour)++count;return count;
}

void test_stand_settings(const std::filesystem::path& assets) {
    const auto path=std::filesystem::temp_directory_path()/
        ("toliss_oans_stands_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(path);
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{path};
    auto write=[&](const char* file,bool mixed,const std::string& name) {
        std::ofstream out(path/file);
        out<<"1 0 0 0 ZSTD Stand test\n1302 icao_code ZSTD\n"
           <<"100 45 1 0 0.25 0 0 0 09 30.0 120.0 0 0 0 0 0 0 27 30.0 120.035 0 0 0 0 0 0\n";
        for(int i=0;i<4;++i)out<<"1300 30.002 "<<120.001+i*.002<<" 90 gate jets "<<(i?std::string("Cargo")+char('A'+i):name)<<"\n1301 "<<char('C'+i)<<" airline\n";
        if(mixed)out<<"1300 30.002 120.010 90 gate jets Small A\n1301 A airline\n"
                     <<"1300 30.002 120.012 90 gate jets Small B\n1301 B airline\n"
                     <<"1300 30.002 120.014 90 gate jets Unknown\n"
                     <<"1300 30.002 120.016 90 gate jets Invalid\n1301 Z airline\n";
        out<<"99\n";
    };
    write("mixed.dat",true,"new ramp 32L");write("large.dat",false,"new ramp 32L");write("short.dat",false,"32L");
    auto load=[&](const char* file){oans::IndexEntry e;e.file=path/file;e.icao="ZSTD";return std::make_unique<oans::A350Map>(assets,e);};
    auto mixed=load("mixed.dat"),large=load("large.dat"),short_name=load("short.dat");
    if(mixed->stats().stand_labels!=4||mixed->stats().stand_areas!=large->stats().stand_areas)
        throw std::runtime_error("C-F stand filter did not remove small/unknown/invalid stands before geometry generation");
    auto equal=[](const auto& a,const auto& b) {
        if(a.size()!=b.size())return false;
        for(size_t i=0;i<a.size();++i)if(a[i].x!=b[i].x||a[i].y!=b[i].y||a[i].u!=b[i].u||a[i].v!=b[i].v||a[i].rgba!=b[i].rgba)return false;
        return true;
    };
    oans::A350View view;view.lat=30.002;view.lon=120.001;view.scale=1;view.range=.2;
    view.center=oans::local(view.lat,view.lon,mixed->ref_lat(),mixed->ref_lon());
    const auto simple=mixed->render(0,view);
    if(!equal(simple,large->render(0,view))||!equal(simple,short_name->render(0,view)))
        throw std::runtime_error("Simple stand text or filtered map differs from expected geometry");
    const auto builds=mixed->cache_stats(0).builds;
    view.simple_stand_names=false;const auto complex=mixed->render(0,view);
    if(equal(simple,complex)||mixed->cache_stats(0).builds!=builds+1)
        throw std::runtime_error("Stand name mode did not update label geometry and invalidate render cache");
    view.simple_stand_names=true;
    if(!equal(simple,mixed->render(0,view)))throw std::runtime_error("Simple mode does not restore original label geometry");
    for(const auto& names:std::vector<std::pair<std::string,std::string>>{
        {"M21","M21"},{"ABC1234","ABC1234"},{"ABCD1234","ABCD1234"},
        {u8"机位M1234",u8"机位M1234"},{u8"机位MN1234",u8"机位MN1234"},
        {"Aircraft North","North"},{"Park Stand_12","Stand_12"},
        {"new ramp start 1","1"},{"new ramp start M7","M7"},
        {"start M7","start M7"},{"new ramp startM7289","7289"}}) {
        write("input.dat",false,names.first);write("expected.dat",false,names.second);
        auto input=load("input.dat"),expected=load("expected.dat");
        if(!equal(input->render(0,view),expected->render(0,view)))
            throw std::runtime_error("Simple stand name length boundary differs from expected label geometry");
    }
    // Different sides can retain opposite naming modes. A cache hit must not
    // prevent the next rebuilt side from selecting its own text variant.
    view.simple_stand_names=false;const auto full_right=mixed->render(1,view);
    view.simple_stand_names=true;const auto simple_left=mixed->render(0,view);
    view.center.x+=2;mixed->render(0,view);
    view.simple_stand_names=false;view.center.x-=2;
    if(!equal(full_right,mixed->render(1,view)))throw std::runtime_error("Other-side name cache changed on a cache hit");
    view.center.x+=2;const auto rebuilt_full=mixed->render(1,view);
    view.simple_stand_names=true;const auto rebuilt_simple=mixed->render(0,view);
    if(equal(rebuilt_full,rebuilt_simple)||simple_left.empty())throw std::runtime_error("Side-specific name selection leaked");
    // Instrument only the temporary Lua fixture: the UTF-8 name scan must run
    // while loading, and must never run during a redraw or a settings switch.
    const auto monitored_assets=path/"assets";
    std::filesystem::create_directories(monitored_assets);
    std::filesystem::copy(assets/"a350",monitored_assets/"a350",std::filesystem::copy_options::recursive);
    {
        std::ofstream out(monitored_assets/"a350/map_engine.lua",std::ios::app);
        out<<R"lua(
local original_gsub, scans = string.gsub, 0
string.gsub = function(s, pattern, ...)
    if pattern == '[^\128-\191]' then scans = scans+1 end
    return original_gsub(s, pattern, ...)
end
local original_load = a350_load
a350_load = function(...)
    local result = table.pack(original_load(...))
    assert(scans > 0, 'stand names were not prepared during load')
    return table.unpack(result, 1, result.n)
end
local original_render = a350_render
a350_render = function(...)
    local before = scans
    local result = table.pack(original_render(...))
    assert(scans == before, 'stand names rescanned during rendering')
    return table.unpack(result, 1, result.n)
end
)lua";
    }
    oans::IndexEntry monitored_entry;monitored_entry.file=path/"mixed.dat";monitored_entry.icao="ZSTD";
    oans::A350Map monitored(monitored_assets,monitored_entry);
    for(bool simple_mode:{true,true,false,false,true}) {
        view.simple_stand_names=simple_mode;view.center.x+=2;monitored.render(0,view);
    }
    std::cout<<"PASS C-F stand geometry filter, simple/complex labels and render cache refresh\n";
}

void test_building_edges(const std::filesystem::path& assets) {
    const auto path=std::filesystem::temp_directory_path()/
        ("toliss_oans_buildings_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::create_directory(path);
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove_all(path,ec);}} cleanup{path};
    {
        std::ofstream out(path/"apt.dat");
        out<<"1 0 0 0 ZBLD Building edge probe\n1302 icao_code ZBLD\n"
           <<"100 45 1 0 0.25 0 0 0 01 47.7500000 129.0200000 0 0 0 0 0 0 19 47.7535000 129.0202450 0 0 0 0 0 0\n99\n";
    }
    // Four touching terminal wings surround a real courtyard. An additional
    // blue building checks that both palette classes receive the same fix.
    const std::array<std::array<double,4>,5> boxes={{{-100,-100,100,-60},
        {-100,60,100,100},{-100,-60,-60,60},{60,-60,100,60},{140,-30,200,30}}};
    const double lat=47.7518,lon=129.0006;
    std::ofstream dsf(path/"dsf.json"),osm(path/"osm.json");
    dsf<<std::setprecision(15)<<"{\"schema\":\"oans-dsf-buildings-v1\",\"buildings\":[";
    osm<<std::setprecision(15)<<"{\"elements\":[";
    for(size_t i=0;i<boxes.size();++i) {
        const auto& b=boxes[i];if(i){dsf<<',';osm<<',';}
        dsf<<"{\"kind\":\""<<(i<4?"terminal":"building")<<"\",\"points\":[";
        osm<<"{\"id\":"<<i+1<<",\"type\":\"way\",\"tags\":{"<<(i<4?"\"aeroway\":\"terminal\"":"\"building\":\"yes\"")<<"},\"geometry\":[";
        for(int k=0;k<5;++k) {
            const double x=(k==1||k==2)?b[2]:b[0],y=(k==2||k==3)?b[3]:b[1];
            const double la=lat+y/oans::earth*180/oans::pi,lo=lon+x/(oans::earth*std::cos(lat*oans::pi/180))*180/oans::pi;
            if(k){dsf<<',';osm<<',';}dsf<<'['<<lo<<','<<la<<']';osm<<"{\"lon\":"<<lo<<",\"lat\":"<<la<<'}';
        }
        dsf<<"]}";osm<<"]}";
    }
    dsf<<"]}";osm<<"]}";dsf.close();osm.close();
    oans::IndexEntry entry;entry.file=path/"apt.dat";entry.icao="ZBLD";
    oans::A350Map facade(assets,entry,path/"dsf.json"),overpass(assets,entry,path/"osm.json");
    auto covers=[](const std::vector<oans::A350Vertex>& vs,oans::Vec2 p) {
        auto cross=[](const auto& a,const auto& b,oans::Vec2 q){return (b.x-a.x)*(q.y-a.y)-(b.y-a.y)*(q.x-a.x);};
        for(size_t k=0;k+2<vs.size();k+=3) {
            const auto& a=vs[k];const auto& b=vs[k+1];const auto& c=vs[k+2];
            if((a.rgba&0xffffff)!=0xf0d200)continue;
            const double d=cross(a,b,{c.x,c.y});if(std::abs(d)<1e-6)continue;
            const double u=cross(a,b,p),v=cross(b,c,p),w=cross(c,a,p);
            if((u>=0&&v>=0&&w>=0)||(u<=0&&v<=0&&w<=0))return true;
        }
        return false;
    };
    for(int mode:{2,3,4})for(bool panel:{false,true})for(double angle:{0.,.7})for(double range:{.2,.5,1.}) {
        oans::A350View view;view.lat=lat;view.lon=lon;view.mode=mode;view.angle=angle;view.range=range;
        const auto vp=oans::map_viewport(mode,panel);view.anchor=vp.anchor;view.radius=vp.radius;view.bottom=vp.bottom;
        view.scale=vp.radius/(range*1852);view.center=oans::local(lat,lon,facade.ref_lat(),facade.ref_lon());
        const auto a=facade.render(0,view),b=overpass.render(0,view);
        if(a.size()!=b.size())throw std::runtime_error("OSM/DSF building edge pipeline differs");
        for(size_t k=0;k<a.size();++k)if(a[k].x!=b[k].x||a[k].y!=b[k].y||a[k].rgba!=b[k].rgba)
            throw std::runtime_error("OSM/DSF equivalent footprints differ");
        if(colour_count(a,0x00f0d200)==0||(view.scale/2>=.05&&colour_count(a,0x00c8641e)==0))
            throw std::runtime_error("Terminal or blue building lacks transparent AA edge vertices");
        oans::MapProjection projection;projection.center=view.center;projection.anchor=vp.anchor;projection.scale=view.scale;projection.angle=angle;
        if(covers(a,vp.anchor)||!covers(a,projection.to(view.center+oans::Vec2{-80,0})))
            throw std::runtime_error("Building courtyard filled or terminal wing missing");
    }
    std::cout<<"PASS 36 building views: equivalent DSF/OSM, cyan/blue AA, courtyard clear and terminal wing filled\n";
}

void test_manual_marks_and_tower(const std::filesystem::path& assets) {
    const auto path=std::filesystem::temp_directory_path()/
        ("toliss_oans_symbols_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".apt.dat");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
    auto fixture=[&](const std::string& row) {
        std::ofstream out(path,std::ios::binary);
        out<<"1 0 0 0 ZSYM Reference symbols\n1302 icao_code ZSYM\n"
           <<"100 45 1 0 0.25 0 0 0 01 47.7500000 129.0000000 0 0 0 0 0 0 19 47.7535000 129.0002450 0 0 0 0 0 0\n"
           <<row<<"\n99\n";
        if(!out)throw std::runtime_error("Cannot write reference-symbol fixture");
    };
    oans::IndexEntry entry;entry.file=path;entry.icao="ZSYM";
    fixture("");oans::A350Map base(assets,entry);
    oans::A350View view;
    view.lat=47.7502;view.lon=129.003;view.psi=35;view.range=.2;
    view.center=oans::local(47.7518,129.0006,base.ref_lat(),base.ref_lon());
    for(int mode:{2,3,4})for(bool panel:{false,true}) {
        const auto viewport=oans::map_viewport(mode,panel);
        view.mode=mode;view.anchor=viewport.anchor;view.radius=viewport.radius;view.bottom=panel?204:40;
        view.scale=view.radius/(view.range*1852);view.angle=mode==4?0:view.psi*oans::pi/180;
        const auto plain=base.render(0,view);
        auto crosses=view;crosses.crosses={view.center+oans::Vec2{-80,0}};
        const auto white=base.render(0,crosses);
        if(colour_count(white,0xFFFFFFFF)<=colour_count(plain,0xFFFFFFFF)||
           colour_count(white,0xFF00E6FF)!=colour_count(plain,0xFF00E6FF))
            throw std::runtime_error("Pilot cross must add white geometry without yellow geometry");
        auto flags=view;flags.flags={view.center+oans::Vec2{80,0}};
        const auto green=base.render(0,flags);
        if(colour_count(green,0xFF00FF00)<=colour_count(plain,0xFF00FF00)||
           colour_count(green,0xFFFF40FF)!=colour_count(plain,0xFFFF40FF))
            throw std::runtime_error("Pilot flag must add green geometry without magenta geometry");
        for(const auto& row:std::array<std::string,7>{
            "14 47.7518 129.0006 100 0 ATC Tower", "14 47.7518 129.0006 100 0",
            "14 47.7518 129.0006 100 0 TWR", "14 47.7518 129.0006 100 0 Observation deck",
            "14 147.7518 129.0006 100 0 Tower", "14 bad 129.0006 100 0 Tower", "18 47.7518 129.0006 1 BCN"}) {
            fixture(row);oans::A350Map tower(assets,entry);
            const auto vertices=tower.render(0,view);
            const bool valid=row.find("ATC Tower")!=std::string::npos||row.back()=='0'||row.find(" TWR")!=std::string::npos;
            if((colour_count(vertices,0xFF00FF00)>colour_count(plain,0xFF00FF00))!=valid)
                throw std::runtime_error("Tower must use a valid unnamed/tower-named row 14 only");
            const auto position=tower.tower();
            if(position.has_value()!=valid||(position&&(std::abs(position->lat-47.7518)>1e-9||std::abs(position->lon-129.0006)>1e-9)))
                throw std::runtime_error("Native TWR selector must expose the same accepted tower coordinate as the map");
            if(valid) {
                size_t near=0;
                for(const auto& v:vertices)if(v.rgba==0xFF00FF00&&
                    std::abs(v.x-view.anchor.x)<30&&v.y>view.anchor.y-45&&v.y<view.anchor.y+16)++near;
                if(near<20)throw std::runtime_error("Tower is not at its projected geographic position");
                auto away=view;away.center.x+=5000;
                const auto offscreen=tower.render(0,away);
                if(colour_count(offscreen,0xFF00FF00)!=colour_count(base.render(0,away),0xFF00FF00))
                    throw std::runtime_error("Offscreen tower leaks into the viewport");
                auto covered=view;covered.center.y+=1000;
                const auto clipped=tower.render(0,covered);
                if(colour_count(clipped,0xFF00FF00)!=colour_count(base.render(0,covered),0xFF00FF00))
                    throw std::runtime_error("Tower leaks across the header or lower panel");
                if(mode==4&&!panel&&row.find("ATC Tower")!=std::string::npos) {
                    auto marked=view;marked.crosses=crosses.crosses;marked.flags=flags.flags;
                    const auto preview=tower.render(0,marked);
                    std::filesystem::create_directories("reports/runtime/reference-symbols");
                    std::ofstream out("reports/runtime/reference-symbols/marks-tower.bin",std::ios::binary);
                    out.write(reinterpret_cast<const char*>(preview.data()),preview.size()*sizeof(oans::A350Vertex));
                    if(!out)throw std::runtime_error("Cannot write reference-symbol preview");
                }
            }
        }
    }
    oans::A350Map ui(assets);
    if(colour_count(ui.advisory(3,false,{{"01","19",2,50}},"",true),0xFF00E6FF)==0)
        throw std::runtime_error("RWY AHEAD must retain its yellow warning colour");
    std::cout<<"PASS white crosses, green flags, yellow advisory and tower source/projection/clipping in NAV/ARC/PLAN, panels open/closed\n";
}

void test_ils_line_styles(const std::filesystem::path& assets) {
    const auto path=std::filesystem::temp_directory_path()/
        ("toliss_oans_ils_markings_"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".apt.dat");
    struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{path};
    constexpr std::uint32_t yellow=0xFF00D4F2,red=0xFF2828E6;
    for(int style:std::array<int,8>{6,56,7,57,11,61,14,64}) {
        std::cout<<"Checking synthetic ILS style "<<style<<std::endl;
        {
            std::ofstream out(path,std::ios::binary);
            out<<"1 0 0 0 ZILS ILS line-style probe\n"
               <<"1302 icao_code ZILS\n"
               <<"100 45 1 0 0.25 0 0 0 01 47.7500000 129.0000000 0 0 0 0 0 0 19 47.7535000 129.0002450 0 0 0 0 0 0\n"
               <<"120 ILS line style "<<style<<"\n"
               <<"111 47.7518000 129.0000000 "<<style<<"\n"
               <<"115 47.7518000 129.0012000\n99\n";
            if(!out)throw std::runtime_error("Cannot write ILS line-style fixture");
        }
        oans::IndexEntry entry;entry.file=path;entry.offset=0;entry.icao="ZILS";
        oans::A350Map map(assets,entry);
        for(double range:{.35,.10}) {
            std::cout<<"  range "<<range<<" NM"<<std::endl;
            oans::A350View view;view.lat=47.7518;view.lon=129.0006;view.psi=view.mag=0;view.angle=0;
            view.range=range;view.mode=3;view.scale=370/(range*1852);view.anchor={300,145};view.radius=370;view.bottom=0;
            view.center=oans::local(view.lat,view.lon,map.ref_lat(),map.ref_lon());
            const auto& vertices=map.render(0,view);
            const auto yellow_n=colour_count(vertices,yellow),red_n=colour_count(vertices,red);
            if(yellow_n<6)
                throw std::runtime_error("ILS line style must render its basic yellow path at medium and close range");
            if(red_n>0)throw std::runtime_error("ILS marking was misrendered as an ordinary red hold line");
        }
    }
    std::cout<<"PASS basic yellow ILS Hold/Critical Centerline types 6/56/7/57/11/61/14/64 at medium and close range\n";
}
}

// Exercise the actual upstream engine against installed apt.dat, without a sim.
// Output is the exact triangle/UV/colour buffer subsequently submitted by .xpl.
int main(int argc,char** argv) {
    if(argc<5) {std::cerr<<"Usage: a350_map_probe ASSETS XPLANE_ROOT ICAO OUTPUT_BUFFER [OSM] [LAT LON PSI RANGE MODE]\n";return 2;}
    try {
        // UI-only state has no airport, scenery, or OSM dependency.
        oans::A350Map ui(std::filesystem::u8path(argv[1]));
        for(bool on:{false,true}) {
            const auto& button=ui.oans_switch(on);
            if(button.vertices.empty()||button.bounds[0]>=button.bounds[2]||button.bounds[1]>=button.bounds[3])
                throw std::runtime_error("Missing A350 OANS checkbox geometry");
            for(const auto& v:button.vertices)
                if(v.x<button.bounds[0]-3||v.x>button.bounds[2]+3||v.y<button.bounds[1]-3||v.y>button.bounds[3]+3)
                    throw std::runtime_error("Checkbox draws outside its own small overlay");
        }
        if(ui.oans_switch(true).vertices.size()<=ui.oans_switch(false).vertices.size())
            throw std::runtime_error("OANS tick is missing");
        std::cout<<"PASS original A350 checkbox loads without airport; checked tick and bounded overlay\n";
        const auto cursor=ui.cursor({300,300});
        if(cursor.empty())throw std::runtime_error("Missing map cursor");
        for(const auto& v:cursor)if((v.rgba&0xffffff)!=0xff00ff||v.x<283||v.x>317||v.y<278||v.y>322)
            throw std::runtime_error("Cursor must be bounded vertical magenta chevrons and centre dot");
        if(ui.plan_notice(4,true).empty()||!ui.plan_notice(4,false).empty()||
           !ui.plan_notice(2,true).empty()||!ui.plan_notice(3,true).empty())
            throw std::runtime_error("Non-plan airport notice leaks across ND modes or membership");
        std::cout<<"PASS magenta cursor geometry and PLAN-only airport notice\n";
        test_ils_line_styles(std::filesystem::u8path(argv[1]));
        test_stand_settings(std::filesystem::u8path(argv[1]));
        test_building_edges(std::filesystem::u8path(argv[1]));
        test_manual_marks_and_tower(std::filesystem::u8path(argv[1]));
        auto write_buffer=[](const std::filesystem::path& path,const std::vector<oans::A350Vertex>& buffer) {
            std::filesystem::create_directories(path.parent_path());std::ofstream out(path,std::ios::binary);
            out.write(reinterpret_cast<const char*>(buffer.data()),buffer.size()*sizeof(oans::A350Vertex));
            if(!out)throw std::runtime_error("Advisory preview write failed");
        };
        std::vector<oans::RunwayAheadHit> threats={{"16R","34L",3,100},{"04","22",2,50}};
        auto green_count=[](const auto& vertices){size_t n=0;for(const auto& v:vertices)if(v.rgba==0xFF00FF00)++n;return n;};
        if(green_count(ui.advisory(3,false,threats,"",true))!=0||green_count(ui.advisory(3,false,threats,"16R",true))==0||
           green_count(ui.advisory(3,false,threats,"016R",true))==0||green_count(ui.advisory(3,false,threats,"34L",true))==0)
            throw std::runtime_error("FMS endpoint colour or normalization failure");
        for(int mode:{2,3,4}) {
            const auto overlay=ui.advisory(mode,false,threats,"16R",true);
            if(overlay.empty()||!ui.advisory(mode,false,threats,"16R",false).empty()||!ui.advisory(mode,false,{},"",true).empty())
                throw std::runtime_error("Advisory blink/clear failure");
            for(const auto& p:overlay)if(p.x<0||p.x>600||p.y<0||p.y>600)throw std::runtime_error("Advisory outside ND");
            if(mode==3)for(const auto& p:overlay)if(p.y<270||p.y>375)throw std::runtime_error("ARC advisory is not at manual-relative height");
            write_buffer("reports/runtime/0.4.6/advisory-mode-"+std::to_string(mode)+".bin",overlay);
        }
        const auto pfd=ui.advisory(3,true,threats,"",true);
        if(pfd.empty())throw std::runtime_error("Missing PFD advisory");
        for(const auto& p:pfd)if(p.x<150||p.x>450||p.y<320||p.y>370)throw std::runtime_error("PFD advisory must lie above the attitude horizontal reference");
        write_buffer("reports/runtime/0.4.6/advisory-pfd.bin",pfd);
        std::cout<<"PASS ARC/ROSE dual-runway, PLAN and PFD overlays; blink and clear\n";
        oans::Airport crossing;crossing.icao="CROSS";
        crossing.runways={{"05","23",{-1500,-1000},{1500,1000},45},
                          {"14","32",{-1000,1000},{1000,-1000},45}};
        const std::vector<oans::RunwayAheadHit> crossing_hits={{"05","23",3,120},{"14","32",2,60}};
        for(int mode:{2,3})for(bool southbound:{false,true}) {
            const auto ordered=oans::orient_runway_advisories(&crossing,crossing_hits,southbound?180:0);
            const auto vertices=ui.advisory(mode,false,ordered,"32",true);
            double left=600,right=0;
            for(const auto& v:vertices)if(v.rgba==0xFF00FF00){left=std::min(left,double(v.x));right=std::max(right,double(v.x));}
            if(left>right||(southbound?(left+right)/2>=300:(left+right)/2<=300))
                throw std::runtime_error("Green 32 advisory label is on the wrong side of its actual threshold");
            if(mode==3&&southbound)write_buffer("reports/runtime/reference-symbols/advisory-left-32.bin",vertices);
        }
        std::cout<<"PASS approaching crossing runways: 32-14 with green 32 on left; opposite approach 14-32 with green 32 on right\n";
        std::vector<oans::IndexEntry> index;
        for(const auto& path:oans::scenery_files(std::filesystem::u8path(argv[2]))) {
            auto more=oans::scan_index(path);index.insert(index.end(),more.begin(),more.end());
        }
        auto entry=oans::find_airport(oans::canonical_index(index),argv[3]);if(!entry)throw std::runtime_error("Airport absent");
        auto before=std::chrono::steady_clock::now();
        oans::A350Map engine(std::filesystem::u8path(argv[1]),*entry,argc>5?std::filesystem::u8path(argv[5]):std::filesystem::path{});
        auto loaded=std::chrono::steady_clock::now();
        auto st=engine.stats();
        oans::A350View view;
        view.lat=argc>6?std::stod(argv[6]):engine.ref_lat();view.lon=argc>7?std::stod(argv[7]):engine.ref_lon();
        view.psi=view.mag=argc>8?std::stod(argv[8]):0;view.range=argc>9?std::stod(argv[9]):.2;view.mode=argc>10?std::stoi(argv[10]):3;
        view.angle=view.mode==4?0:view.psi*oans::pi/180;view.scale=(view.mode==3?370:245)/(view.range*1852);
        view.anchor={300,view.mode==3?145.:285.};view.center=oans::local(view.lat,view.lon,engine.ref_lat(),engine.ref_lon());
        const auto& vertices=engine.render(0,view);
        auto rendered=std::chrono::steady_clock::now();
        std::ofstream out(std::filesystem::u8path(argv[4]),std::ios::binary);
        out.write(reinterpret_cast<const char*>(vertices.data()),vertices.size()*sizeof(oans::A350Vertex));
        if(!out)throw std::runtime_error("Cannot write output buffer");
        auto geo_offset=[&](double east,double north) {
            return oans::Geo{view.lat+north/oans::earth*180/oans::pi,
                view.lon+east/(oans::earth*std::cos(view.lat*oans::pi/180))*180/oans::pi};
        };
        const auto close_a=geo_offset(150,80),close_b=geo_offset(-180,-100),fast=geo_offset(200,-150),far=geo_offset(1900,0);
        std::vector<oans::TcasTrafficSample> samples={
            {0,999,view.lat,view.lon,view.psi,0},
            {1,1001,close_a.lat,close_a.lon,view.psi+90,30},
            {2,1002,close_b.lat,close_b.lon,view.psi+180,40},
            {3,1003,fast.lat,fast.lon,view.psi,80*oans::kt},
            {4,1004,far.lat,far.lon,view.psi,0},
            {5,0,close_a.lat,close_a.lon,view.psi,0},
            {6,1006,close_a.lat,close_a.lon,std::numeric_limits<double>::quiet_NaN(),0}
        };
        const auto selected=oans::filter_tcas_traffic(samples,view.lat,view.lon);
        if(selected.size()!=2||selected[0].slot!=1||selected[1].slot!=2)
            throw std::runtime_error("TCAS filter must keep only valid non-ownship targets within 1 NM and below 80 kt");
        auto traffic_view=view;traffic_view.traffic=selected;
        const auto& traffic_vertices=engine.render(2,traffic_view);
        const auto white_delta=static_cast<long long>(colour_count(traffic_vertices,0xFFFFFFFF))-
                              static_cast<long long>(colour_count(vertices,0xFFFFFFFF));
        if(white_delta!=static_cast<long long>(selected.size())*36) {
            std::ostringstream error;error<<"Selected traffic must add 36 white symbol vertices each: delta="<<white_delta
                <<" targets="<<selected.size();throw std::runtime_error(error.str());
        }
        auto traffic_path=std::filesystem::u8path(argv[4]);
        traffic_path.replace_filename(traffic_path.stem().string()+"-traffic.bin");
        write_buffer(traffic_path,traffic_vertices);
        const auto traffic_vertex_count=traffic_vertices.size();
        auto hidden_view=traffic_view;hidden_view.position_valid=false;
        const auto hidden=engine.render(2,hidden_view);
        if(colour_count(vertices,0xFFFF5AFF)==0||colour_count(hidden,0xFFFF5AFF)!=0)
            throw std::runtime_error("Unavailable position must remove the actual magenta ownship geometry");
        auto hidden_without_traffic=hidden_view;hidden_without_traffic.traffic.clear();
        const auto hidden_plain=engine.render(2,hidden_without_traffic);
        if(hidden.size()!=hidden_plain.size()||colour_count(hidden,0xFFFFFFFF)!=colour_count(hidden_plain,0xFFFFFFFF))
            throw std::runtime_error("Unavailable position must suppress traffic even if stale targets reach Lua");
        const auto& restored=engine.render(2,traffic_view);
        if(colour_count(restored,0xFFFF5AFF)!=colour_count(vertices,0xFFFF5AFF)||restored.size()!=traffic_vertex_count)
            throw std::runtime_error("Position validity changes must invalidate cached hidden aircraft symbols");
        std::cout<<"PASS unavailable position hides ownship and traffic in production Lua, including cached valid/invalid transitions\n";
        const auto cached=engine.render(0,view).data();
        if(cached!=vertices.data())throw std::runtime_error("View cache miss");
        const auto before_jitter=engine.cache_stats(0);
        for(int i=0;i<1000;++i){auto jitter=view;jitter.lat+=(i%3)*1e-8;jitter.psi+=(i%3)*1e-5;
            jitter.angle+=(i%3)*1e-7;engine.render(0,jitter);}
        if(engine.cache_stats(0).builds!=before_jitter.builds)throw std::runtime_error("Stationary subpixel jitter rebuilds the map");
        auto moved=view;moved.center.x+=.5/view.scale;engine.render(0,moved);
        if(engine.cache_stats(0).builds!=before_jitter.builds+1)throw std::runtime_error("Real map motion was incorrectly cached");
        std::cout<<"PASS 1000 stationary jitter frames reuse buffer; 0.5 pixel motion rebuilds\n";
        // Check the original FMS QFU triangle at an actual installed runway end.
        const auto airport=oans::load_airport(*entry);const auto& runway=airport.runways.front();
        if(entry->icao=="ZUCK"&&airport.iata!="CKG")throw std::runtime_error("Installed ZUCK metadata did not supply CKG");
        std::cout<<"Airport codes: "<<airport.icao<<" "<<airport.iata<<'\n';
        const auto ref=oans::local(airport.lat,airport.lon,engine.ref_lat(),engine.ref_lon());
        for(int mode:{2,3,4})for(bool panel:{false,true})for(double along:{.1,.5,.9})for(double angle:{0.,.55,2.6,3.7,5.7}) {
            auto partial=view;partial.mode=mode;partial.angle=angle;partial.range=.2;
            const auto vp=oans::map_viewport(mode,panel);
            partial.anchor=vp.anchor;partial.radius=vp.radius;partial.bottom=vp.bottom;
            partial.scale=vp.radius/(partial.range*1852);
            partial.center=ref+runway.start+(runway.end-runway.start)*along;
            partial.fms_airport=entry->icao;partial.fms_runway=runway.reciprocal;
            oans::MapProjection projection;projection.center=partial.center;projection.anchor=partial.anchor;
            projection.scale=partial.scale;projection.angle=partial.angle;
            const auto source_start=projection.to(ref+runway.start),source_end=projection.to(ref+runway.end);
            const bool forward=source_start.x<=source_end.x;
            const auto& expected_first=forward?runway.name:runway.reciprocal;
            const auto& expected_second=forward?runway.reciprocal:runway.name;
            const auto buffer=engine.render(1,partial);
            bool found=false;
            for(const auto& label:engine.runway_labels(1))if((label.first==runway.name&&label.second==runway.reciprocal)||
                                                                           (label.first==runway.reciprocal&&label.second==runway.name)) {
                found=true;
                if(label.first!=expected_first||label.second!=expected_second)
                    throw std::runtime_error("Runway pair names are reversed relative to their projected physical endpoints");
                const double direction=oans::dot(label.end-label.start,source_end-source_start);
                if(forward?direction<=0:direction>=0)
                    throw std::runtime_error("Runway label diagnostic endpoints do not follow the displayed names");
                if(label.selected!=runway.reciprocal||oans::length(label.center-(label.start+label.end)*.5)>1e-5)
                    throw std::runtime_error("Runway ident is not at visible segment midpoint or selected endpoint missing");
                if(label.center.x<0||label.center.x>600||label.center.y<partial.bottom||label.center.y>550)
                    throw std::runtime_error("Runway ident covered by header/panel");
            }
            if(!found||green_count(buffer)==0)throw std::runtime_error("Partial runway lost selected green ident");
            if(mode==4&&!panel&&along==.1&&angle==0)write_buffer("reports/runtime/0.4.8/partial-selected.bin",buffer);
            partial.fms_runway.clear();partial.map_runway=runway.name;
            if(green_count(engine.render(1,partial))==0)throw std::runtime_error("Manual runway selection is not green");
            partial.map_runway.clear();
            if(green_count(engine.render(1,partial))!=0)throw std::runtime_error("Deselected runway leaves stale green ident");
        }
        std::cout<<"PASS 90 partial-runway mode/panel/position/angle cases: physical endpoint order, visible midpoint, FMS/manual green endpoint, deselection cache\n";
        // Reproduce the user's Pudong 35L/35R screenshot, then turn the view
        // through 180 degrees: names must follow the actual projected ends.
        if(entry->icao=="ZSPD") {
            std::vector<const oans::Runway*> parallels;
            for(const auto& r:airport.runways)if(r.name=="35L"||r.name=="35R")parallels.push_back(&r);
            if(parallels.size()!=2)throw std::runtime_error("ZSPD reference requires 35L and 35R");
            for(int mode:{2,3,4})for(bool reversed:{false,true}) {
                auto v=view;v.mode=mode;v.range=1;v.angle=-.65+(reversed?oans::pi:0);
                const auto vp=oans::map_viewport(mode,false);
                v.anchor=vp.anchor;v.radius=vp.radius;v.bottom=vp.bottom;v.scale=vp.radius/(v.range*1852);
                v.center=ref+(parallels[0]->start+parallels[1]->start)*.5+oans::Vec2{0,900};
                const auto preview=engine.render(1,v);
                int found=0;
                for(const auto& label:engine.runway_labels(1))for(const auto* r:parallels)
                    if((label.first==r->name&&label.second==r->reciprocal)||(label.second==r->name&&label.first==r->reciprocal)) {
                        if(label.first!=(reversed?r->reciprocal:r->name)||label.second!=(reversed?r->name:r->reciprocal))
                            throw std::runtime_error("Pudong 35L/35R pair is reversed relative to its threshold");
                        ++found;
                    }
                if(found!=2)throw std::runtime_error("Pudong parallel-runway reference labels missing");
                if(mode==2&&!reversed)write_buffer("reports/runtime/reference-symbols/runway-order-zspd.bin",preview);
            }
            std::cout<<"PASS Pudong 35L-17R and 35R-17L, plus opposite view direction, in NAV/ARC/PLAN\n";
        }
        auto fms_view=view;fms_view.mode=4;fms_view.angle=0;fms_view.anchor={300,285};
        fms_view.center=oans::local(airport.lat,airport.lon,engine.ref_lat(),engine.ref_lon())+runway.start;
        fms_view.range=.5;fms_view.scale=245/(fms_view.range*1852);
        // The threshold placard stays white; FMS selection adds a solid green
        // triangle there, while the separate midfield pair may turn green.
        auto nearby_text=[](const auto& vertices,oans::Vec2 center,std::uint32_t colour) {
            size_t count=0;
            for(size_t i=0;i+2<vertices.size();i+=3) {
                const auto& a=vertices[i];const auto& b=vertices[i+1];const auto& c=vertices[i+2];
                const bool textured=a.u!=b.u||a.v!=b.v||a.u!=c.u||a.v!=c.v;
                if(textured&&a.rgba==colour&&oans::length(oans::Vec2{a.x,a.y}-center)<25&&
                   oans::length(oans::Vec2{b.x,b.y}-center)<25&&oans::length(oans::Vec2{c.x,c.y}-center)<25)count+=3;
            }
            return count;
        };
        for(int mode:{2,3,4})for(bool panel:{false,true})for(double angle:{0.,2.6}) {
            auto placard=fms_view;const auto vp=oans::map_viewport(mode,panel);
            placard.mode=mode;placard.angle=angle;placard.anchor=vp.anchor;
            placard.radius=vp.radius;placard.bottom=vp.bottom;placard.scale=vp.radius/(placard.range*1852);
            oans::MapProjection projection;projection.center=placard.center;projection.anchor=vp.anchor;
            projection.scale=placard.scale;projection.angle=angle;
            const auto inward=projection.to(ref+runway.end)-vp.anchor;
            const auto label_center=vp.anchor-inward*(29.08/oans::length(inward));
            const auto plain=engine.render(1,placard);
            placard.fms_airport=entry->icao;placard.fms_runway=runway.name;
            const auto selected=engine.render(1,placard);
            const auto white=nearby_text(plain,label_center,0xFFFFFFFF);
            if(!white||nearby_text(selected,label_center,0xFFFFFFFF)!=white||
               nearby_text(selected,label_center,0xFF00FF00)!=nearby_text(plain,label_center,0xFF00FF00)||
               green_count(selected)<=green_count(plain)) {
                std::ostringstream error;error<<"FMS threshold placard must retain white text beside its green triangle: mode="<<mode
                    <<" panel="<<panel<<" angle="<<angle<<" white="<<white<<" -> "<<nearby_text(selected,label_center,0xFFFFFFFF)
                    <<" green_text="<<nearby_text(plain,label_center,0xFF00FF00)<<" -> "<<nearby_text(selected,label_center,0xFF00FF00);
                throw std::runtime_error(error.str());
            }
            if(mode==2&&!panel&&angle==2.6)write_buffer("reports/runtime/reference-symbols/runway-white-placard.bin",selected);
        }
        std::cout<<"PASS 12 threshold placard cases: white text retained with green FMS triangle in NAV/ARC/PLAN, panels open/closed, rotated views\n";
        const auto base_count=engine.render(2,fms_view).size();
        fms_view.fms_airport=entry->icao;fms_view.fms_runway=runway.name;
        const auto marker_count=engine.render(2,fms_view).size();
        if(marker_count<=base_count)throw std::runtime_error("Original FMS runway triangle is missing");
        auto detail=engine.render(2,fms_view);
        auto pointer_overlay=ui.cursor({380,275});detail.insert(detail.end(),pointer_overlay.begin(),pointer_overlay.end());
        auto notice_overlay=ui.plan_notice(4,true);detail.insert(detail.end(),notice_overlay.begin(),notice_overlay.end());
        write_buffer("reports/runtime/0.4.7/qfu-cursor-notice.bin",detail);
        auto detail_heading=fms_view;detail_heading.mode=2;detail_heading.angle=-.55;
        write_buffer("reports/runtime/0.4.7/qfu-rotated.bin",engine.render(1,detail_heading));
        const auto before_cursor=engine.cache_stats(2).builds;
        ui.cursor({370,280});engine.render(2,fms_view);
        if(engine.cache_stats(2).builds!=before_cursor)throw std::runtime_error("Pointer motion rebuilds airport map");
        const auto fms_builds=engine.cache_stats(2).builds;engine.render(2,fms_view);
        if(engine.cache_stats(2).builds!=fms_builds)throw std::runtime_error("Unchanged FMS runway breaks map cache");
        fms_view.fms_airport="XXXX";
        if(engine.render(2,fms_view).size()!=base_count)throw std::runtime_error("FMS marker leaks to unrelated airport");
        fms_view.fms_airport=entry->icao;fms_view.fms_runway.clear();
        if(engine.render(2,fms_view).size()!=base_count)throw std::runtime_error("Clearing FMS runway leaves stale triangle");
        std::cout<<"PASS original FMS runway triangle, airport matching, cache invalidation and clearing\n";
        fms_view.runway_ahead={{runway.name,runway.reciprocal,5,50}};fms_view.alert_blink=false;
        const auto dim_count=engine.render(2,fms_view).size();
        const auto before_alert=engine.cache_stats(2).builds;
        fms_view.alert_blink=true;
        auto alert_map=engine.render(2,fms_view);
        if(alert_map.size()<=dim_count||engine.cache_stats(2).builds!=before_alert+1)throw std::runtime_error("Runway fill blink/cache failure");
        auto warning=ui.advisory(fms_view.mode,false,fms_view.runway_ahead,"",true);
        alert_map.insert(alert_map.end(),warning.begin(),warning.end());
        write_buffer("reports/runtime/0.4.6/runway-highlight.bin",alert_map);
        fms_view.runway_ahead.clear();
        if(engine.render(2,fms_view).size()!=base_count)throw std::runtime_error("Clearing advisory leaves stale runway highlight");
        std::cout<<"PASS original runway polygon highlight, blink cache and removal\n";
        std::cout<<"source="<<entry->file.u8string()<<" offset="<<entry->offset<<"\n"
            <<"pavements="<<st.pavements<<" stand_areas="<<st.stand_areas<<" stand_labels="<<st.stand_labels
            <<" terminals="<<st.terminals<<" buildings="<<st.buildings<<" vertices="<<vertices.size()<<"\n"
            <<"load_ms="<<std::chrono::duration<double,std::milli>(loaded-before).count()
            <<" render_ms="<<std::chrono::duration<double,std::milli>(rendered-loaded).count()<<" cache=PASS\n";
        if(argc>11&&std::string(argv[11])=="--matrix") {
            for(int mode:{2,3,4})for(double range:{.2,.5,1.,2.,5.})for(bool panel:{false,true}) {
                auto v=view;v.mode=mode;v.range=range;v.angle=mode==4?0:v.psi*oans::pi/180;
                const auto viewport=oans::map_viewport(mode,panel);v.anchor=viewport.anchor;
                v.radius=viewport.radius;v.bottom=viewport.bottom;v.scale=v.radius/(range*1852);
                auto start=std::chrono::steady_clock::now();
                const auto& result=engine.render(1,v);
                if(result.size()<100)throw std::runtime_error("Empty matrix map");
                auto count=result.size();auto front=result.front();
                // The pilot's marks must invalidate the cache, then removing
                // them must restore the same deterministic original output.
                v.crosses={v.center};v.flags={v.center+oans::Vec2{30,30}};
                if(engine.render(1,v).size()<=count)throw std::runtime_error("Pilot marks not added");
                v.crosses.clear();v.flags.clear();const auto& restored=engine.render(1,v);
                if(restored.size()!=count||restored.front().x!=front.x||restored.front().y!=front.y)
                    throw std::runtime_error("Map state leaked across views/marks");
                std::cout<<"PASS mode="<<mode<<" range="<<range<<" panel="<<panel<<" vertices="<<count<<" build_and_two_mark_changes_ms="
                    <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()<<'\n';
            }
        }
        return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
