#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace oans {
constexpr double pi = 3.14159265358979323846, kt = 0.514444444444, earth = 6371000;
struct Vec2 {
    double x = 0, y = 0;
    Vec2 operator+(Vec2 p) const { return {x+p.x,y+p.y}; }
    Vec2 operator-(Vec2 p) const { return {x-p.x,y-p.y}; }
    Vec2 operator*(double f) const { return {x*f,y*f}; }
};
double dot(Vec2 a, Vec2 b);
double length(Vec2 a);
Vec2 local(double lat,double lon,double lat0,double lon0);
struct Geo { double lat = 0, lon = 0; };
struct Runway {
    std::string name, reciprocal;
    Vec2 start, end;
    double width = 0, displaced = 0;
    double stopway_start = 0, stopway_end = 0; // apt.dat blast/overrun beyond physical ends
    double length() const;
    Vec2 axis() const;
    Vec2 station(Vec2 p) const; // along runway from physical end, right of centerline
};
struct Polygon { int surface = 1; std::vector<std::vector<Vec2>> rings; };
struct Stand { Vec2 pos; double heading = 0; std::string name; char category = 0; };
inline std::string stand_name(const std::string& name,bool simple=true) {
    auto count=[](const std::string& text) {
        size_t characters=0;
        for(unsigned char c:text)if((c&0xc0)!=0x80)++characters;
        return characters;
    };
    if(!simple||count(name)<=8)return name;
    const auto end=name.find_last_not_of(' ');
    if(end==std::string::npos)return name;
    const auto space=name.find_last_of(' ',end);
    const auto last=name.substr(space==std::string::npos?0:space+1,end-(space==std::string::npos?0:space+1)+1);
    if(count(last)<=8)return last;
    const auto digit=last.find_first_of("0123456789");
    return digit!=std::string::npos?last.substr(digit):last;
}
struct Edge { int a = 0, b = 0; bool runway = false; bool oneway = false; std::string name; };
struct PaintedLine { int style = 0; std::vector<Vec2> points; };
struct Airport {
    // icao is the explicit 1302 code, or header_id as a local lookup fallback.
    // The fallback must not be presented as a confirmed ICAO.
    std::string icao, header_id, name, iata;
    bool icao_explicit = false;
    double lat = 0, lon = 0; // Geometry projection origin.
    Geo reference; // apt.dat ARP, or longest-runway midpoint when absent.
    std::vector<Runway> runways;
    std::vector<Polygon> pavements;
    std::vector<Stand> stands;
    std::map<int,Vec2> nodes;
    std::vector<Edge> edges;
    std::vector<PaintedLine> markings;
};
struct RunwaySize { double length = 0, width = 0; };
// Actual runway rectangle, then stand within 20 m, then heading-aware taxi route
// within 15 m (including anonymous routes). Empty for anonymous/unknown or
// names longer than five UTF-8 characters; never fall through to another name.
std::string surface_name(const Airport& airport,Vec2 position,double heading,bool simple_stands=true);
// Call at 2 Hz. Taxi-route heading, 4 m preference for the current route and
// 0.5 s confirmation prevent name flicker at intersections. Anonymous routes
// retain their identity even though their displayed name is empty.
class SurfaceTracker {
    const Airport* airport_=nullptr;
    std::string name_,pending_;
    int kind_=0;
    bool pending_valid_=false;
    double pending_since_=0,last_sample_=-1;
    Vec2 last_position_;
public:
    void reset();
    std::string update(const Airport& airport,Vec2 position,double heading,double now,bool simple_stands=true);
};
struct IndexEntry {
    std::filesystem::path file;
    std::int64_t offset = 0;
    std::string icao, header_id, name;
    double lat = 0, lon = 0, longest = 0;
    bool icao_explicit = false;
    std::string iata = {};
    std::string city = {};
    std::vector<RunwaySize> runway_sizes = {};
};
std::vector<std::filesystem::path> scenery_files(const std::filesystem::path& root);
std::vector<IndexEntry> scan_index(const std::filesystem::path& path,const std::atomic<bool>* stop=nullptr);
Airport read_airport(std::istream& input);
Airport load_airport(const IndexEntry& entry);
// Scenery/file order wins; within a file explicit 1302 records beat header fallbacks.
std::vector<IndexEntry> canonical_index(const std::vector<IndexEntry>& index);
double minimum_runway_length(const std::string& aircraft_icao);
double minimum_runway_width(const std::string& aircraft_icao);
// Apply after canonical_index so a rejected scenery cannot reveal a lower-priority duplicate.
std::vector<IndexEntry> filter_airport_index(const std::vector<IndexEntry>& index,double minimum_length,double minimum_width=0);
std::optional<IndexEntry> find_airport(const std::vector<IndexEntry>& index,const std::string& code);
constexpr double airport_search_radius = 40 * 1852.0; // 40 nautical miles, in metres.
std::optional<IndexEntry> nearest(const std::vector<IndexEntry>& index,double lat,double lon,double radius=airport_search_radius);
// Projective mapping: corners are top left, top right, bottom right, bottom left.
struct Homography {
    std::array<double,9> h{};
    Vec2 map(Vec2 p) const;
    std::optional<Homography> inverse() const;
    static std::optional<Homography> quad(const std::array<Vec2,4>& corners);
};
}
