#pragma once
#include "core.hpp"
#include <cmath>

namespace oans {
// A320 host viewport only. The original A350 map/compass consume the same
// anchor and radius; picking and dragging use this identical projection.
struct MapViewport {Vec2 anchor;double radius,bottom;};
inline MapViewport map_viewport(int mode,bool panel,double height=202) {
    // Panels occlude the map; they must not move or rescale its projection.
    // ARC labels sit outside the ring. Reserve 52 pixels below the header
    // (y=552) for the large rotated captions, including their atlas padding.
    const double bottom=panel?height+2:40;
    return mode==3?MapViewport{{300,160},340,bottom}:MapViewport{{300,285},245,bottom};
}
struct MapProjection {
    Vec2 center,anchor{300,285};double scale=1,angle=0,radius=245,bottom=40,width=600;
    Vec2 to(Vec2 p) const {p=p-center;double c=std::cos(angle),s=std::sin(angle);return {anchor.x+scale*(p.x*c-p.y*s),anchor.y+scale*(p.x*s+p.y*c)};}
    Vec2 from(Vec2 p) const {p={(p.x-anchor.x)/scale,(p.y-anchor.y)/scale};double c=std::cos(angle),s=std::sin(angle);return center+Vec2{p.x*c+p.y*s,-p.x*s+p.y*c};}
};
}
