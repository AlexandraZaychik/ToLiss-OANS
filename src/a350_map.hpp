#pragma once
#include "core.hpp"
#include "runway_ahead.hpp"
#include <memory>

namespace oans {
// Geometry and drawing use extracted A350 Lua with recorded host edits. This host
// owns a private Lua state; it never accesses FlyWithLua's state or callbacks.
struct A350Vertex {float x,y,u,v;std::uint32_t rgba;};
struct TcasTrafficSample {int slot=0,mode_s_id=0;double lat=0,lon=0,psi=0,speed_mps=0;};
struct A350Traffic {int slot=0;double lat=0,lon=0,psi=0;};
std::vector<A350Traffic> filter_tcas_traffic(const std::vector<TcasTrafficSample>& samples,
                                             double own_lat,double own_lon);
struct A350View {
    Vec2 center,anchor{300,285};
    double scale=1,angle=0,lat=0,lon=0,psi=0,mag=0,range=1;
    double radius=0,bottom=40; // zero radius selects the legacy closed viewport
    int mode=4;
    std::string fms_airport,fms_runway,map_runway;
    std::vector<Vec2> crosses,flags;
    std::vector<A350Traffic> traffic;
    std::vector<RunwayAheadHit> runway_ahead;
    bool alert_blink=false;
    bool outside_plan=false;
    bool position_valid=true;
    bool simple_stand_names=true;
    bool reusable_with(const A350View& o) const;
    bool same_map_content(const A350View& o) const;
    bool base_reusable_with(const A350View& o) const;
};
struct A350Stats {int pavements=0,stand_areas=0,stand_labels=0,terminals=0,buildings=0;};
// first/second and start/end follow the displayed text direction.
struct A350RunwayLabel {std::string first,second,selected;Vec2 center,start,end;};
struct A350CacheStats {std::uint64_t builds=0,hits=0;double last_ms=0,total_ms=0;};
struct A350Switch {
    std::vector<A350Vertex> vertices;
    std::array<double,4> bounds{}; // left, bottom, right, top in the host's 600 square
};
class A350Map {
    struct Impl;
    std::unique_ptr<Impl> p_;
    const std::vector<A350Vertex>& render_impl(int side,const A350View& view,int layer);
public:
    explicit A350Map(const std::filesystem::path& assets); // UI without airport geometry
    A350Map(const std::filesystem::path& assets,const IndexEntry& entry,
            const std::filesystem::path& buildings={},const std::atomic<bool>* cancel=nullptr);
    ~A350Map();
    A350Map(const A350Map&)=delete;
    A350Map& operator=(const A350Map&)=delete;
    const std::vector<A350Vertex>& render(int side,const A350View& view);
    // Motion-only refreshes: >= 1 virtual pixel, 10 Hz. Stagger simultaneous
    // sides by one frame; content/interaction/coverage bypass the timer.
    const std::vector<A350Vertex>& render_base(int side,const A350View& view,double now,bool interactive=false,
                                              std::int64_t frame=-1);
    const A350View& base_view(int side) const;
    // Borrowed output remains valid until the next call to the same method
    // (or render/render_base side 3 for dynamic). Other output buffers are independent.
    const std::vector<A350Vertex>& render_dynamic(const A350View& view);
    const std::vector<A350RunwayLabel>& runway_labels(int side) const;
    const A350Switch& oans_switch(bool on) const;
    const std::vector<A350Vertex>& cursor(Vec2 at,bool horizontal=false,bool yellow=false);
    const std::vector<A350Vertex>& plan_notice(int mode,bool outside);
    const std::vector<A350Vertex>& advisory(int mode,bool pfd,const std::vector<RunwayAheadHit>& hits,
                                    const std::string& fms_runway,bool blink);
    A350Stats stats() const;
    A350CacheStats cache_stats(int side) const;
    // Same accepted apt.dat viewpoint used by the rendered TWR symbol.
    std::optional<Geo> tower() const;
    double ref_lat() const;
    double ref_lon() const;
};
}
