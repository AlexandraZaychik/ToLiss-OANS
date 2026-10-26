#include "dsf_buildings.hpp"
#include <cmath>
#include <chrono>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace fs=std::filesystem;
using Bytes=std::string;
void check(bool condition,const char* message){if(!condition)throw std::runtime_error(message);}
void u16(Bytes& b,unsigned n){b+=char(n);b+=char(n>>8);}
void u32(Bytes& b,unsigned n){u16(b,n);u16(b,n>>16);}
void f32(Bytes& b,float n){unsigned v;std::memcpy(&v,&n,4);u32(b,v);}
Bytes atom(const char* id,const Bytes& b){Bytes out(id,4);u32(out,unsigned(b.size()+8));return out+b;}
Bytes string_table(std::initializer_list<const char*> strings){Bytes out;for(auto s:strings){out+=s;out+='\0';}return out;}
void write(const fs::path& p,const Bytes& b){fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<b;check(bool(f),"write fixture");}
Bytes dsf(bool exclude=false,bool filtered=false,bool curved=false,float base_lat=29.70f,float base_lon=106.64f) {
    auto props=string_table({"sim/overlay","1"});
    if(exclude)props+=string_table({"sim/exclude_fac","106.5/29.5/106.9/29.9"});
    if(filtered)props+=string_table({"sim/filter/aptid","TEST"});
    const auto defs=atom("TRET",{})+atom("TJBO",{})+atom("YLOP",string_table({"lib/airport/Terminal_kit/term_building_test.fac"}))+atom("WTEN",{});
    Bytes pool;u32(pool,4);pool+=char(curved?4:2);
    // Uncompressed planes; pool scale=0 stores these lon/lat as raw integers.
    // Use fractional coords through scales below, a ~100m square near ZUCK.
    const unsigned coords[]={10000,11000,11000,10000,10000,10000,11000,11000};
    for(int plane=0;plane<(curved?4:2);++plane){pool+=char(0);for(int i=0;i<4;++i)u16(pool,coords[(plane%2)*4+i]);}
    Bytes scale;for(int plane=0;plane<(curved?4:2);++plane){f32(scale,.065535f);f32(scale,plane%2?base_lat:base_lon);}
    Bytes commands;
    if(filtered){commands+=char(32);commands+=char(6);u16(commands,1);u32(commands,0);}
    commands+=char(1);u16(commands,0);commands+=char(3);commands+=char(0);
    commands+=char(12);u16(commands,12);commands+=char(4);for(unsigned i=0;i<4;++i)u16(commands,i);
    Bytes b="XPLNEDSF";u32(b,1);
    Bytes empty32;u32(empty32,0);empty32+=char(0); // Legal empty pool in real Global Airports tiles.
    b+=atom("DAEH",atom("PORP",props))+atom("NFED",defs)+atom("DOEG",atom("LOOP",pool)+atom("LACS",scale)+atom("23OP",empty32)+atom("23CS",{}))+atom("SDMC",commands);
    b.append(16,'\0');return b;
}
int main(int argc,char** argv) {
    const auto root=fs::temp_directory_path()/fs::u8path(u8"oans-runtime-dsf-tests-\u5730\u666f");
    try {
        if(argc==3&&std::string(argv[1])=="--fixture") {
            write(fs::u8path(argv[2]),dsf(false,false,false,29.991f,119.999f));return 0;
        }
        // Each run gets a fresh directory; recursive cleanup is intentionally
        // avoided so a failed test leaves its inspectable inputs/output.
        const auto run=root/std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
        const auto high=run/"Custom Scenery/High",low=run/"Custom Scenery/Low",off=run/"Custom Scenery/Disabled";
        const fs::path tile="Earth nav data/+20+100/+29+106.dsf";
        const auto apt=high/"Earth nav data/apt.dat";
        write(apt,"I\n1200\n1 0 0 0 TEST Test airport\n1302 icao_code TEST\n100 45 1 0 0.25 1 1 0 01 29.70 106.64 0 0 0 0 0 0 19 29.73 106.68 0 0 0 0 0 0\n99\n");
        auto entries=oans::scan_index(apt);check(entries.size()==1,"airport fixture parsed");
        const auto& e=entries[0];std::atomic<bool> cancel{false};
        write(run/"Custom Scenery/scenery_packs.ini","I\n1000\nSCENERY_PACK Custom Scenery/High/\nSCENERY_PACK_DISABLED Custom Scenery/Disabled/\nSCENERY_PACK Custom Scenery/Low/\nSCENERY_PACK *GLOBAL_AIRPORTS*\n");
        write(high/tile,dsf());write(low/tile,dsf());write(off/tile,dsf());
        auto packs=oans::dsf_scenery_packs(run);check(packs[0]==high&&packs[1]==low,"active scenery priority and disabled pack");
        auto r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);
        check(r.buildings==2&&r.terminals==2&&r.tiles==2&&fs::exists(r.cache),"direct binary DSF extraction");
        write(high/tile,dsf(true));
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.buildings==1,"higher-priority facade exclusion applies only below");
        write(high/tile,dsf());write(low/tile,dsf(false,true));
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.buildings==1,"airport filter hides overridden lower airport");
        write(high/tile,dsf(false,true,true));
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.buildings==1,"owner airport filter retained, curve-depth pool supported");
        auto broken=dsf();const auto commands=broken.find("SDMC");check(commands!=Bytes::npos,"fixture commands located");
        broken[commands+9]=char(255);write(high/tile,broken);
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.cache.empty(),"bad point pool selection falls back safely");
        broken=dsf();broken[broken.size()-18]=char(255);write(high/tile,broken);
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.cache.empty(),"bad vertex index falls back safely");
        broken=dsf();broken[broken.find("LOOP")+8]=char(255);write(high/tile,broken);
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.cache.empty(),"truncated encoded pool falls back safely");
        broken=dsf();broken[4]='X';broken.resize(30);write(high/tile,broken);
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.cache.empty(),"truncated atom falls back safely");
        write(high/tile,"corrupt DSF file with at least 28 bytes");
        r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.buildings==0&&r.cache.empty(),"invalid DSF does not reuse stale JSON");
        cancel=true;r=oans::extract_runtime_dsf(run,e,run/"cache",cancel);check(r.cache.empty(),"cooperative cancellation");
        std::cout<<"Runtime DSF tests passed: binary decode, priority, disabled pack, exclusions, airport filter, curve pool, corrupt pool/index/atom, invalid/stale cache, cancellation\n";
        return 0;
    }catch(const std::exception& e){std::cerr<<"DSF test failure: "<<e.what()<<"\n";return 1;}
}
