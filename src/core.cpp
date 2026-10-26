#include "core.hpp"
#include "aircraft_limits.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>
#include <fstream>
#include <limits>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string_view>

namespace oans {
double dot(Vec2 a,Vec2 b) { return a.x*b.x+a.y*b.y; }
double length(Vec2 a) { return std::hypot(a.x,a.y); }
Vec2 local(double lat,double lon,double lat0,double lon0) {
    return {std::remainder(lon-lon0,360.0)*pi/180*earth*std::cos(lat0*pi/180),(lat-lat0)*pi/180*earth};
}
double Runway::length() const { return oans::length(end-start); }
Vec2 Runway::axis() const { const double n=length(); return n>0?(end-start)*(1/n):Vec2{}; }
Vec2 Runway::station(Vec2 p) const { auto u=axis(), d=p-start; return {dot(d,u),dot(d,{u.y,-u.x})}; }
namespace {
enum {surface_none,surface_runway,surface_stand,surface_taxi};
struct SurfaceMatch {int kind=surface_none;std::string name;};
std::string surface_label(const std::string& name) {
    size_t characters=0;
    for(unsigned char c:name)if((c&0xc0)!=0x80)++characters;
    return characters<=5?name:std::string{};
}
SurfaceMatch match_surface(const Airport& a,Vec2 p,double heading,bool simple_stands,const std::string* preferred=nullptr) {
    if(!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(heading))return {};
    const double h=heading*pi/180;const Vec2 forward{std::sin(h),std::cos(h)};
    const Runway* runway=nullptr;double alignment=-1;
    for(const auto& r:a.runways) {
        if(r.length()<1||r.width<=0)continue;
        const auto at=r.station(p);
        if(at.x<0||at.x>r.length()||std::abs(at.y)>r.width*.5)continue;
        const double match=std::abs(dot(r.axis(),forward));
        if(match>alignment) {alignment=match;runway=&r;}
    }
    if(runway)return {surface_runway,dot(runway->axis(),forward)>=0?runway->name:runway->reciprocal};
    const Stand* stand=nullptr;double best=20;
    for(const auto& s:a.stands)if(!s.name.empty()) {
        const double d=length(p-s.pos);if(d<=best) {best=d;stand=&s;}
    }
    if(stand)return {surface_stand,stand_name(stand->name,simple_stands)};
    const Edge* taxi=nullptr;best=INFINITY;
    for(const auto& e:a.edges) {
        if(e.runway)continue; // Anonymous taxiways still own their position.
        const auto start=a.nodes.find(e.a),end=a.nodes.find(e.b);
        if(start==a.nodes.end()||end==a.nodes.end())continue;
        const auto d=end->second-start->second;const double size=dot(d,d);
        if(size<1e-6)continue;
        const double t=dot(p-start->second,d)/size;
        if(t<0||t>1)continue; // Do not extend a routing edge beyond its ends.
        const double distance=length(p-(start->second+d*t));
        if(distance>15)continue;
        // At a crossing, proximity alone cannot distinguish the two routes.
        // Both directions of a taxiway are valid for identification.
        const double alignment=std::min(1.0,std::abs(dot(d,forward))/std::sqrt(size));
        const double score=distance+12*(1-alignment)-(preferred&&e.name==*preferred?4:0);
        const bool tie=std::abs(score-best)<1e-6;
        if(score<best-1e-6||(tie&&(!taxi||e.name<taxi->name))) {best=score;taxi=&e;}
    }
    return taxi?SurfaceMatch{surface_taxi,taxi->name}:SurfaceMatch{};
}
}
std::string surface_name(const Airport& a,Vec2 p,double heading,bool simple_stands) {
    return surface_label(match_surface(a,p,heading,simple_stands).name);
}
void SurfaceTracker::reset() {
    airport_=nullptr;name_.clear();pending_.clear();kind_=surface_none;
    pending_valid_=false;pending_since_=0;last_sample_=-1;last_position_={};
}
std::string SurfaceTracker::update(const Airport& a,Vec2 p,double heading,double now,bool simple_stands) {
    if(!std::isfinite(now)||!std::isfinite(p.x)||!std::isfinite(p.y)||!std::isfinite(heading)) {reset();return {};}
    if(airport_!=&a||now<last_sample_||length(p-last_position_)>100)reset();
    airport_=&a;last_sample_=now;last_position_=p;
    const auto next=match_surface(a,p,heading,simple_stands,kind_==surface_taxi?&name_:nullptr);
    if(kind_==surface_taxi&&next.kind==surface_taxi&&next.name!=name_) {
        if(!pending_valid_||pending_!=next.name) {pending_=next.name;pending_since_=now;pending_valid_=true;}
        if(now-pending_since_<.5)return surface_label(name_);
    }
    kind_=next.kind;name_=next.name;pending_valid_=false;pending_.clear();
    return surface_label(name_);
}
namespace {
std::vector<std::string> tokens(const std::string& s) {
    std::istringstream in(s); std::vector<std::string> t; std::string v;
    while(in>>v) t.push_back(v);
    return t;
}
double num(const std::vector<std::string>& t,size_t i,double fallback=0) {
    if(i>=t.size()) return fallback;
    try { size_t n; double v=std::stod(t[i],&n); return n==t[i].size()&&std::isfinite(v)?v:fallback; }
    catch(...) { return fallback; }
}
std::string tail(const std::vector<std::string>& t,size_t i) {
    std::string s; for(;i<t.size();++i) { if(!s.empty())s+=' '; s+=t[i]; } return s;
}
bool header(const std::vector<std::string>& t) { return t.size()>4&&(t[0]=="1"||t[0]=="16"||t[0]=="17"); }
std::string upper(std::string s) { for(char& c:s)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c))); return s; }
std::vector<const IndexEntry*> preferred_entries(const std::vector<IndexEntry>& entries) {
    std::vector<const IndexEntry*> out;
    std::map<std::string,size_t> positions;
    for(const auto& e:entries) {
        auto [it,inserted]=positions.emplace(e.icao,out.size());
        if(inserted)out.push_back(&e);
        else if(e.file==out[it->second]->file&&e.icao_explicit&&!out[it->second]->icao_explicit)out[it->second]=&e;
    }
    return out;
}
}
std::vector<std::filesystem::path> scenery_files(const std::filesystem::path& root) {
    std::vector<std::filesystem::path> out; std::set<std::filesystem::path> seen;
    const auto custom=root/"Custom Scenery";
    const auto global=root/"Global Scenery/Global Airports/Earth nav data/apt.dat";
    const auto legacy_global=custom/"Global Airports/Earth nav data/apt.dat";
    auto add=[&](std::filesystem::path p) {
        std::error_code ec;
        p=p.lexically_normal(); if(std::filesystem::is_regular_file(p,ec)&&seen.insert(p).second)out.push_back(p);
    };
    auto pack_path=[&](const std::string& value) {
        auto p=std::filesystem::u8path(value);if(p.is_relative())p=root/p;
        return (p/"Earth nav data/apt.dat").lexically_normal();
    };
    std::set<std::filesystem::path> disabled;
    std::ifstream in(custom/"scenery_packs.ini"); std::string line;
    while(std::getline(in,line)) {
        const bool inactive=line.rfind("SCENERY_PACK_DISABLED ",0)==0;
        if(!inactive&&line.rfind("SCENERY_PACK ",0)!=0)continue;
        auto v=line.substr(inactive?22:13);
        const auto end=v.find_last_not_of(" \t\r");if(end==std::string::npos)continue;
        v.resize(end+1);
        if(v=="*GLOBAL_AIRPORTS*")continue;
        auto p=pack_path(v);
        if(inactive)disabled.insert(p);
        // Built-in airports are fallback sources even when their marker is
        // above custom airports in scenery_packs.ini.
        else if(p!=global.lexically_normal()&&p!=legacy_global.lexically_normal())add(p);
    }
    // New scenery may not have been registered by X-Plane yet. Append it in a
    // stable order, while retaining configured priority and disabled packs.
    std::vector<std::filesystem::path> unlisted;
    std::error_code ec;
    for(std::filesystem::directory_iterator it(custom,ec),end;it!=end;it.increment(ec)) {
        if(ec)break;
        auto p=(it->path()/"Earth nav data/apt.dat").lexically_normal();
        if(p!=legacy_global.lexically_normal()&&!disabled.count(p))unlisted.push_back(p);
    }
    std::sort(unlisted.begin(),unlisted.end());
    for(const auto& p:unlisted)add(p);
    add(global);
    if(!disabled.count(legacy_global.lexically_normal()))add(legacy_global);
    add(root/"Resources/default scenery/default apt dat/Earth nav data/apt.dat");
    return out;
}
std::vector<IndexEntry> scan_index(const std::filesystem::path& path,const std::atomic<bool>* stop) {
    std::ifstream in(path,std::ios::binary); if(!in)throw std::runtime_error("Cannot open apt.dat");
    std::vector<IndexEntry> out; std::optional<IndexEntry> cur; std::string line; size_t n=0;
    std::optional<double> datum_lat,datum_lon;
    auto finish=[&] {
        if(cur&&cur->longest>0) {
            if(datum_lat&&datum_lon) {cur->lat=*datum_lat;cur->lon=*datum_lon;}
            out.push_back(*cur);
        }
        cur.reset();datum_lat.reset();datum_lon.reset();
    };
    while(in) {
        const auto off=in.tellg(); if(!std::getline(in,line))break;
        if((++n%2048)==0&&stop&&stop->load())break;
        // Skip the millions of pavement points while indexing.
        auto first=line.find_first_not_of(" \t");if(first==std::string::npos)continue;
        auto end=line.find_first_of(" \t\r",first);if(end==std::string::npos)end=line.size();
        std::string_view code(line.data()+first,end-first);
        if(code!="1"&&code!="16"&&code!="17"&&code!="100"&&code!="1302"&&code!="99")continue;
        auto t=tokens(line); if(t.empty())continue;
        if(header(t)) {
            finish(); if(t[0]=="1")cur=IndexEntry{path,static_cast<std::int64_t>(off),upper(t[4]),t[4],tail(t,5),0,0,0};
        } else if(t[0]=="99")finish();
        else if(cur&&t[0]=="1302"&&t.size()>2&&t[1]=="icao_code") {cur->icao=upper(t[2]);cur->icao_explicit=true;}
        else if(cur&&t[0]=="1302"&&t.size()>2&&t[1]=="iata_code")cur->iata=upper(t[2]);
        else if(cur&&t[0]=="1302"&&t.size()>2&&t[1]=="city"&&tail(t,2)!="(unassigned)")cur->city=tail(t,2);
        else if(cur&&t[0]=="1302"&&t.size()>2&&t[1]=="datum_lat") {
            const double v=num(t,2,999);datum_lat=std::abs(v)<=90?std::optional<double>(v):std::nullopt;
        }
        else if(cur&&t[0]=="1302"&&t.size()>2&&t[1]=="datum_lon") {
            const double v=num(t,2,999);datum_lon=std::abs(v)<=180?std::optional<double>(v):std::nullopt;
        }
        else if(cur&&t[0]=="100"&&t.size()>=26) {
            const double lat=num(t,9,999),lon=num(t,10,999),lat2=num(t,18,999),lon2=num(t,19,999);
            if(std::abs(lat)>90||std::abs(lat2)>90||std::abs(lon)>180||std::abs(lon2)>180)continue;
            double d=length(local(lat2,lon2,lat,lon));
            cur->runway_sizes.push_back({d,num(t,1)});
            if(d>cur->longest) { cur->lat=(lat+lat2)*.5;cur->lon=std::remainder(lon+std::remainder(lon2-lon,360.0)*.5,360.0);cur->longest=d; }
        }
    }
    finish(); return out;
}
Airport read_airport(std::istream& in) {
    Airport a; std::string line; std::vector<std::vector<std::string>> rows; bool started=false;
    while(std::getline(in,line)) {
        auto t=tokens(line); if(t.empty())continue;
        if(header(t)) { if(started)break;started=true;a.header_id=t[4];a.icao=upper(t[4]);a.name=tail(t,5); }
        if(!started)continue;
        if(t[0]=="99")break;
        if(t[0]=="1302"&&t.size()>2&&t[1]=="icao_code") {a.icao=upper(t[2]);a.icao_explicit=true;}
        if(t[0]=="1302"&&t.size()>2&&t[1]=="iata_code")a.iata=upper(t[2]);
        rows.push_back(std::move(t));
    }
    std::optional<double> datum_lat,datum_lon;
    double longest=0;
    for(const auto& t:rows) {
        if(t[0]=="1302"&&t.size()>2) {
            const double v=num(t,2,999);
            if(t[1]=="datum_lat")datum_lat=std::abs(v)<=90?std::optional<double>(v):std::nullopt;
            if(t[1]=="datum_lon")datum_lon=std::abs(v)<=180?std::optional<double>(v):std::nullopt;
        }
        if(t[0]=="100"&&t.size()>=26) {
            const double lat=num(t,9,999),lon=num(t,10,999),lat2=num(t,18,999),lon2=num(t,19,999);
            if(std::abs(lat)>90||std::abs(lat2)>90||std::abs(lon)>180||std::abs(lon2)>180)continue;
            const double d=length(local(lat2,lon2,lat,lon));
            if(d>longest) {longest=d;a.reference={(lat+lat2)*.5,std::remainder(lon+std::remainder(lon2-lon,360.0)*.5,360.0)};}
        }
    }
    if(datum_lat&&datum_lon)a.reference={*datum_lat,*datum_lon};
    bool ref=false;
    for(const auto& t:rows)if(t[0]=="100"&&t.size()>=26) {
        a.lat=num(t,9,999);a.lon=num(t,10,999);ref=std::abs(a.lat)<=90&&std::abs(a.lon)<=180;if(ref)break;
    }
    if(!ref)throw std::runtime_error("Airport has no valid runway");
    auto point=[&](const std::vector<std::string>& t,size_t i) { return local(num(t,i),num(t,i+1),a.lat,a.lon); };
    Polygon* poly=nullptr; std::vector<Vec2>* ring=nullptr;
    Vec2 prev{}, outgoing{},first{},first_incoming{}; bool prev_bezier=false,first_bezier=false;
    for(const auto& t:rows) {
        int code=static_cast<int>(num(t,0,-1));
        if(code==100&&t.size()>=26) {
            if(std::abs(num(t,9,999))>90||std::abs(num(t,18,999))>90||
               std::abs(num(t,10,999))>180||std::abs(num(t,19,999))>180)continue;
            auto s=point(t,9),e=point(t,18); double w=num(t,1);
            if(length(e-s)>1&&w>0) {
                a.runways.push_back({t[8],t[17],s,e,w,num(t,11),num(t,12),num(t,21)});
                a.runways.push_back({t[17],t[8],e,s,w,num(t,20),num(t,21),num(t,12)});
            }
        } else if(code==110) {
            a.pavements.push_back({static_cast<int>(num(t,1,1)),{}});poly=&a.pavements.back();ring=nullptr;
        } else if(code>=111&&code<=114&&poly&&t.size()>=3) {
            Vec2 p=point(t,1);bool bez=(code==112||code==114)&&t.size()>=5;
            Vec2 incoming=bez?point(t,3):p;
            if(!ring) { poly->rings.emplace_back();ring=&poly->rings.back();first=p;first_incoming=incoming;first_bezier=bez; }
            if(!ring->empty()&&(prev_bezier||bez)) {
                Vec2 c1=prev_bezier?outgoing:prev,c2=incoming;
                for(int k=1;k<=12;++k) { double u=k/12.0,v=1-u;ring->push_back(prev*(v*v*v)+c1*(3*v*v*u)+c2*(3*v*u*u)+p*(u*u*u)); }
            } else ring->push_back(p);
            prev=p;outgoing=p*2-incoming;prev_bezier=bez;
            if(code==113||code==114) {
                if(ring->size()>1&&(prev_bezier||first_bezier)) {
                    Vec2 c1=prev_bezier?outgoing:prev,c2=first_bezier?first_incoming:first;
                    for(int k=1;k<=12;++k) {double u=k/12.0,v=1-u;ring->push_back(prev*(v*v*v)+c1*(3*v*v*u)+c2*(3*v*u*u)+first*(u*u*u));}
                }
                ring=nullptr;prev_bezier=false;
            }
        } else if(code==1300&&t.size()>=7)a.stands.push_back({point(t,1),num(t,3),tail(t,6)});
        else if(code==1301&&t.size()>=2&&!a.stands.empty())
            a.stands.back().category=t[1].size()==1?static_cast<char>(std::toupper(static_cast<unsigned char>(t[1][0]))):0;
        else if(code==1201&&t.size()>=5)a.nodes[static_cast<int>(num(t,4))]=point(t,1);
        else if(code==1202&&t.size()>=5)a.edges.push_back({static_cast<int>(num(t,1)),static_cast<int>(num(t,2)),t[4]=="runway",t[3]=="oneway",tail(t,5)});
        else if(code!=1302&&code!=110&&!(code>=111&&code<=114)) { poly=nullptr;ring=nullptr;prev_bezier=false; }
    }
    // Markings are a separate apt.dat layer. ATC routing edges describe a
    // navigation graph, not the position or shape of painted pavement lines.
    bool painted=false,have=false,curved=false,first_curved=false;
    Vec2 last{},control{},begin{},begin_control{};int style=0;
    auto segment=[&](Vec2 end,Vec2 incoming,bool end_curved) {
        // Include the wide line styles (10-14, 60-64) used by large airports.
        if(!have||!((style>=1&&style<=14)||(style>=51&&style<=64)||(style>=20&&style<=22)))return;
        PaintedLine marking{style,{last}};
        if(curved||end_curved)for(int k=1;k<=12;++k) {
            double u=k/12.0,v=1-u;auto c1=curved?control:last,c2=end_curved?incoming:end;
            marking.points.push_back(last*(v*v*v)+c1*(3*v*v*u)+c2*(3*v*u*u)+end*(u*u*u));
        } else marking.points.push_back(end);
        if(length(end-last)>.001||marking.points.size()>2)a.markings.push_back(std::move(marking));
    };
    for(const auto& t:rows) {
        int code=static_cast<int>(num(t,0,-1));
        if(code==110||code==120) {painted=true;have=false;continue;}
        if(code<111||code>116) {painted=false;have=false;continue;}
        if(!painted||t.size()<3)continue;
        bool bez=(code==112||code==114||code==116)&&t.size()>=5;
        Vec2 p=point(t,1),incoming=bez?point(t,3):p;
        segment(p,incoming,bez);
        if(!have) {begin=p;begin_control=incoming;first_curved=bez;}
        last=p;control=p*2-incoming;curved=bez;have=true;
        // The paint code belongs to the segment STARTING at this node.
        // Light strings (101+) are deliberately not interpreted as paint.
        style=(code==115||code==116)?0:static_cast<int>(num(t,bez?5:3));
        if(code==113||code==114) {segment(begin,begin_control,first_curved);have=false;}
        if(code==115||code==116)have=false;
    }
    // Only explicitly classified C-F stands are part of the OANS map.
    a.stands.erase(std::remove_if(a.stands.begin(),a.stands.end(),[](const Stand& s) {
        return s.category<'C'||s.category>'F';
    }),a.stands.end());
    return a;
}
Airport load_airport(const IndexEntry& e) {
    std::ifstream in(e.file,std::ios::binary);if(!in)throw std::runtime_error("Cannot load airport");
    in.seekg(e.offset);auto a=read_airport(in);if(a.iata.empty())a.iata=e.iata;return a;
}
std::vector<IndexEntry> canonical_index(const std::vector<IndexEntry>& entries) {
    std::vector<IndexEntry> out;
    for(const auto* e:preferred_entries(entries))out.push_back(*e);
    // Fill only missing metadata from lower-priority records with the same
    // explicit ICAO; geometry still comes from the preferred scenery.
    std::map<std::string,std::string> iata,city;
    for(const auto& e:entries)if(e.icao_explicit&&!e.iata.empty())iata.emplace(e.icao,e.iata);
    for(const auto& e:entries)if(e.icao_explicit&&!e.city.empty())city.emplace(e.icao,e.city);
    for(auto& e:out)if(e.icao_explicit) {
        if(e.iata.empty())e.iata=iata[e.icao];
        if(e.city.empty())e.city=city[e.icao];
    }
    return out;
}
double minimum_runway_length(const std::string& aircraft_icao) {
    return aircraft_limits::runway_limits_for(upper(aircraft_icao)).minimum_length;
}
double minimum_runway_width(const std::string& aircraft_icao) {
    return aircraft_limits::runway_limits_for(upper(aircraft_icao)).minimum_width;
}
std::vector<IndexEntry> filter_airport_index(const std::vector<IndexEntry>& index,double minimum_length,double minimum_width) {
    std::vector<IndexEntry> out;
    for(const auto& e:index)if((minimum_length==0&&minimum_width==0)||
        std::any_of(e.runway_sizes.begin(),e.runway_sizes.end(),[&](const RunwaySize& r) {
            return r.length>=minimum_length&&r.width>=minimum_width;
        }))out.push_back(e);
    return out;
}
std::optional<IndexEntry> find_airport(const std::vector<IndexEntry>& entries,const std::string& code) {
    std::optional<IndexEntry> selected;
    const auto query=upper(code);
    for(const auto& e:entries)if(e.icao==query) {
        if(!selected)selected=e;
        else if(e.file==selected->file&&e.icao_explicit&&!selected->icao_explicit)selected=e;
    }
    return selected;
}
std::optional<IndexEntry> nearest(const std::vector<IndexEntry>& entries,double lat,double lon,double radius) {
    std::optional<IndexEntry> best;double d=radius;
    for(const auto* e:preferred_entries(entries)) {
        double v=length(local(e->lat,e->lon,lat,lon));if(v<d) {d=v;best=*e;}
    }
    return best;
}
Vec2 Homography::map(Vec2 p) const {
    double w=h[6]*p.x+h[7]*p.y+h[8];if(std::abs(w)<1e-12)return {1e20,1e20};
    return {(h[0]*p.x+h[1]*p.y+h[2])/w,(h[3]*p.x+h[4]*p.y+h[5])/w};
}
std::optional<Homography> Homography::inverse() const {
    const auto& m=h;
    Homography v{{m[4]*m[8]-m[5]*m[7],m[2]*m[7]-m[1]*m[8],m[1]*m[5]-m[2]*m[4],
        m[5]*m[6]-m[3]*m[8],m[0]*m[8]-m[2]*m[6],m[2]*m[3]-m[0]*m[5],
        m[3]*m[7]-m[4]*m[6],m[1]*m[6]-m[0]*m[7],m[0]*m[4]-m[1]*m[3]}};
    double d=m[0]*v.h[0]+m[1]*v.h[3]+m[2]*v.h[6];if(std::abs(d)<1e-12)return {};
    for(double& x:v.h)x/=d;
    return v;
}
std::optional<Homography> Homography::quad(const std::array<Vec2,4>& q) {
    auto a=q[0],b=q[1],c=q[2],d=q[3];double dx=a.x-b.x+c.x-d.x,dy=a.y-b.y+c.y-d.y,g=0,h=0;
    if(std::abs(dx)+std::abs(dy)>1e-10) {
        double x1=b.x-c.x,x2=d.x-c.x,y1=b.y-c.y,y2=d.y-c.y,det=x1*y2-x2*y1;
        if(std::abs(det)<1e-12)return {};
        g=(dx*y2-x2*dy)/det;h=(x1*dy-dx*y1)/det;
    }
    Homography result{{b.x-a.x+g*b.x,d.x-a.x+h*d.x,a.x,b.y-a.y+g*b.y,d.y-a.y+h*d.y,a.y,g,h,1}};
    if(!result.inverse())return {};
    return result;
}
}
