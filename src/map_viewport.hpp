#pragma once
#include "core.hpp"
#include <cmath>

namespace oans {
// A320 host viewport only. The original A350 map/compass consume the same
// anchor and radius; picking and dragging use this identical projection.
struct MapViewport {Vec2 anchor;double radius,bottom;};
inline MapViewport map_viewport(int mode,bool panel,double height=202) {
    if(panel&&height==110)return mode==3?MapViewport{{300,160},370,112}:MapViewport{{300,330},210,112};
    if(panel&&height==150)return mode==3?MapViewport{{300,200},330,152}:MapViewport{{300,350},190,152};
    if(panel)return mode==3?MapViewport{{300,244},290,204}:MapViewport{{300,376},164,204};
    return mode==3?MapViewport{{300,145},370,40}:MapViewport{{300,285},245,40};
}
struct MapProjection {
    Vec2 center,anchor{300,285};double scale=1,angle=0,radius=245,bottom=40;
    Vec2 to(Vec2 p) const {p=p-center;double c=std::cos(angle),s=std::sin(angle);return {anchor.x+scale*(p.x*c-p.y*s),anchor.y+scale*(p.x*s+p.y*c)};}
    Vec2 from(Vec2 p) const {p={(p.x-anchor.x)/scale,(p.y-anchor.y)/scale};double c=std::cos(angle),s=std::sin(angle);return center+Vec2{p.x*c+p.y*s,-p.x*s+p.y*c};}
};
}
