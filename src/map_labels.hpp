#pragma once
#include "nd_ui.hpp"
#include <map>

namespace oans {
// Screen-space label placement. Geometry remains at its exact geographic
// position; only its caption can move to a nearby unoccupied rectangle.
class MapLabels {
    std::vector<Box> boxes_;
    std::map<std::string,std::vector<Vec2>> named_;
public:
    std::optional<Box> place(Vec2 at,double width,double height,const std::string& repeat_key={}) {
        if(width<=0||height<=0)return {};
        if(!repeat_key.empty()) {
            auto previous=named_.find(repeat_key);
            if(previous!=named_.end())for(auto p:previous->second)if(length(at-p)<170)return {};
        }
        const Vec2 offsets[]={{5,4},{5,8+height},{5,-8-height},{-width-5,4}};
        for(auto offset:offsets) {
            Vec2 p=at+offset;Box b{p.x-3,p.y-4,p.x+width+3,p.y+height+3};
            if(b.l<8||b.r>592||b.b<63||b.t>548)continue;
            bool hit=false;for(const auto& other:boxes_)
                if(b.l<other.r+4&&b.r+4>other.l&&b.b<other.t+4&&b.t+4>other.b){hit=true;break;}
            if(hit)continue;
            boxes_.push_back(b);if(!repeat_key.empty())named_[repeat_key].push_back(at);
            return b;
        }
        return {};
    }
};
}
