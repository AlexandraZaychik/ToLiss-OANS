#pragma once
#include "a350_map.hpp"
#include "map_viewport.hpp"

namespace oans {
// Render beyond the ND edges so rotation/translation can sample real geometry
// between base rebuilds. Leave a texel of safety at the FBO boundary.
constexpr double map_texture_margin=64;
inline Vec2 map_cached_point(Vec2 screen,const A350View& source,const A350View& current) {
    MapProjection a,b;
    a.center=source.center;a.anchor=source.anchor;a.scale=source.scale;a.angle=source.angle;
    b.center=current.center;b.anchor=current.anchor;b.scale=current.scale;b.angle=current.angle;
    return a.to(b.from(screen));
}
inline bool map_texture_covers(const A350View& source,const A350View& current) {
    MapProjection a,b;
    a.center=source.center;a.anchor=source.anchor;a.scale=source.scale;a.angle=source.angle;
    b.center=current.center;b.anchor=current.anchor;b.scale=current.scale;b.angle=current.angle;
    // Also stay inside the upstream culling bounds. Enlarging those bounds
    // changes A350's geometry/line simplification budgets even at the same pose.
    const auto cull_center=a.from({300,300});
    const double cull_radius=std::hypot(300.,300.)/source.scale+30;
    for(auto p:std::array<Vec2,4>{{{0,0},{600,0},{600,600},{0,600}}}) {
        const auto world=b.from(p)-cull_center;
        if(std::abs(world.x)>cull_radius||std::abs(world.y)>cull_radius)return false;
        p=map_cached_point(p,source,current);
        if(p.x < -map_texture_margin+1 || p.y < -map_texture_margin+1 ||
           p.x > 600+map_texture_margin-1 || p.y > 600+map_texture_margin-1)return false;
    }
    return true;
}
}
