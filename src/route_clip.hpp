#pragma once
#include "map_viewport.hpp"
#include <algorithm>

namespace oans {
// Reused on the render thread. A clipped convex quad has at most 104 vertices
// after the four rectangle planes and the 96-sided circular viewport.
class RouteClipper {
    std::array<Vec2,96> circle_{};
    Vec2 anchor_{};
    double radius_=-1;
    std::vector<Vec2> polygon_,scratch_;
    static double cross(Vec2 a,Vec2 b){return a.x*b.y-a.y*b.x;}
    void cut(Vec2 a,Vec2 b) {
        if(polygon_.empty())return;
        const auto edge=b-a;
        // Most planes do not cross the quad; avoid copying for those planes.
        bool inside=false,outside=false;
        for(auto p:polygon_) {if(cross(edge,p-a)>=0)inside=true;else outside=true;}
        if(!outside)return;
        if(!inside){polygon_.clear();return;}
        scratch_.clear();
        auto previous=polygon_.back();double before=cross(edge,previous-a);
        for(auto p:polygon_) {
            double after=cross(edge,p-a);
            if((after>=0)!=(before>=0))scratch_.push_back(previous+(p-previous)*(before/(before-after)));
            if(after>=0)scratch_.push_back(p);
            previous=p;before=after;
        }
        polygon_.swap(scratch_);
    }
public:
    RouteClipper(){polygon_.reserve(108);scratch_.reserve(108);}
    const std::vector<Vec2>& clip(const std::array<Vec2,4>& quad,const MapProjection& projection) {
        polygon_.clear();
        // Transform once per quad and reject using its entire ribbon bounds,
        // including its width. Endpoints outside the viewport can still cross it.
        double l=INFINITY,r=-INFINITY,b=INFINITY,t=-INFINITY;
        const double c=std::cos(projection.angle),s=std::sin(projection.angle);
        for(auto p:quad) {
            p=p-projection.center;
            p={projection.anchor.x+projection.scale*(p.x*c-p.y*s),
               projection.anchor.y+projection.scale*(p.x*s+p.y*c)};
            polygon_.push_back(p);
            l=std::min(l,p.x);r=std::max(r,p.x);b=std::min(b,p.y);t=std::max(t,p.y);
        }
        const double dx=std::max({l-projection.anchor.x,0.,projection.anchor.x-r});
        const double dy=std::max({b-projection.anchor.y,0.,projection.anchor.y-t});
        const double left=(600-projection.width)*.5,right=600-left;
        if(r<left||l>right||t<projection.bottom||b>552||
           dx*dx+dy*dy>projection.radius*projection.radius) {polygon_.clear();return polygon_;}
        if(radius_!=projection.radius||anchor_.x!=projection.anchor.x||anchor_.y!=projection.anchor.y) {
            anchor_=projection.anchor;radius_=projection.radius;
            for(size_t i=0;i<circle_.size();++i) {
                const double a=2*pi*i/circle_.size();
                circle_[i]=anchor_+Vec2{std::cos(a),std::sin(a)}*radius_;
            }
        }
        // Keep the original clipping order and inclusive boundaries.
        for(size_t i=0;i<circle_.size()&&!polygon_.empty();++i)cut(circle_[i],circle_[(i+1)%circle_.size()]);
        cut({left,projection.bottom},{right,projection.bottom});cut({right,552},{left,552});
        cut({left,552},{left,0});cut({right,0},{right,552});
        return polygon_;
    }
};
}
