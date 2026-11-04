#pragma once
#include "core.hpp"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <queue>
#include <set>
#include <tuple>

namespace oans {
struct RoutePlan {
    std::vector<Vec2> points;
    std::string error;
    double metres=0;
    std::string start_stand,end_stand;
    explicit operator bool() const {return error.empty()&&points.size()>1;}
};
inline std::string route_key(std::string s) {
    for(char& c:s)c=char(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
inline std::vector<std::string> route_tokens(const std::string& input) {
    std::vector<std::string> out;std::string token;
    for(unsigned char c:input) {
        if(std::isspace(c)||c=='-'||c==','||c==';'||c=='/'||c=='>') {
            if(!token.empty()){out.push_back(route_key(token));token.clear();}
        } else token+=char(c);
    }
    if(!token.empty())out.push_back(route_key(token));
    return out;
}
struct RouteInput {std::string icao,path,error;};
inline RouteInput parse_route_input(const std::string& input) {
    RouteInput out;out.path=input;
    const auto open=input.find('['),close=input.find(']');
    if(open==std::string::npos&&close==std::string::npos)return out;
    if(open==std::string::npos||close==std::string::npos||close<open||
       input.find('[',open+1)!=std::string::npos||input.find(']',close+1)!=std::string::npos) {
        out.error="Use one airport tag: [ICAO].";return out;
    }
    out.icao=route_key(input.substr(open+1,close-open-1));
    if(out.icao.size()!=4||!std::all_of(out.icao.begin(),out.icao.end(),[](unsigned char c){return std::isalnum(c)!=0;})) {
        out.error="Airport tag must contain a four-character ICAO.";return out;
    }
    out.path=input.substr(0,open)+" "+input.substr(close+1);
    return out;
}
// Search a layered directed graph: an edge must carry the current input name;
// moving to the next name is allowed only at a shared apt.dat node.
inline RoutePlan make_route_plan(const Airport& airport,const std::string& input) {
    RoutePlan result;const auto parsed=parse_route_input(input);auto tokens=route_tokens(parsed.path);
    auto fail=[&](std::string why){result.error=std::move(why);return result;};
    if(!parsed.error.empty())return fail(parsed.error);
    if(!parsed.icao.empty()&&parsed.icao!=route_key(airport.icao))return fail("Airport tag does not match loaded airport: "+parsed.icao);
    if(tokens.empty())return fail("Enter a stand / taxiway / RWxx route.");
    if(tokens.size()>64)return fail("Too many route items (maximum 64).");
    struct Endpoint {const Stand* stand=nullptr;const Runway* runway=nullptr;};
    auto endpoint=[&](const std::string& token,Endpoint& ep)->std::string {
        if(token.rfind("RW",0)==0) {
            const auto name=token.substr(token.rfind("RWY",0)==0?3:2);
            for(const auto& r:airport.runways)if(route_key(r.name)==name){ep.runway=&r;return {};}
            return "Unknown runway: "+token;
        }
        // Prefer full/existing simplified matches. Only then try a substring.
        for(int pass=0;pass<2;++pass) {
            if(pass==1&&std::any_of(airport.edges.begin(),airport.edges.end(),[&](const Edge& e){return !e.runway&&route_key(e.name)==token;}))break;
            for(const auto& s:airport.stands) {
                const auto name=route_key(s.name);
                const bool match=pass==0?(name==token||route_key(stand_name(s.name))==token):name.find(token)!=std::string::npos;
                if(!match)continue;
                if(ep.stand)return "Ambiguous stand: "+token;
                ep.stand=&s;
            }
            if(ep.stand)break;
        }
        return {};
    };
    Endpoint start,end;
    auto error=endpoint(tokens.front(),start);if(!error.empty())return fail(error);
    if(tokens.size()>1){error=endpoint(tokens.back(),end);if(!error.empty())return fail(error);}
    if(start.stand)result.start_stand=start.stand->name;
    if(end.stand)result.end_stand=end.stand->name;
    if(start.stand||start.runway)tokens.erase(tokens.begin());
    if((end.stand||end.runway)&&!tokens.empty())tokens.pop_back();
    if(tokens.empty())return fail("Include at least one named taxiway.");
    // A stand joins the nearest point on its specified taxiway, including the
    // interior of an edge. Split that edge locally so routing still obeys its
    // connectivity and direction; never move the airport's actual nodes.
    auto nodes=airport.nodes;
    std::map<size_t,std::vector<std::pair<double,int>>> cuts;
    int virtual_node=-1;
    auto stand_attachment=[&](const Endpoint& ep,const std::string& name)->std::optional<int> {
        if(!ep.stand)return {};
        double best=std::numeric_limits<double>::infinity(),best_t=0;
        std::optional<size_t> selected;
        for(size_t i=0;i<airport.edges.size();++i) {
            const auto& e=airport.edges[i];
            if(e.runway||route_key(e.name)!=name)continue;
            auto a=airport.nodes.find(e.a),b=airport.nodes.find(e.b);
            if(a==airport.nodes.end()||b==airport.nodes.end())continue;
            const auto d=b->second-a->second;const double squared=dot(d,d);
            if(squared<1e-12)continue;
            const double t=std::clamp(dot(ep.stand->pos-a->second,d)/squared,0.0,1.0);
            const double distance=length(ep.stand->pos-(a->second+d*t));
            if(distance<best){best=distance;best_t=t;selected=i;}
        }
        if(!selected)return {};
        const auto& edge=airport.edges[*selected];
        if(best_t<=1e-9)return edge.a;
        if(best_t>=1-1e-9)return edge.b;
        auto& splits=cuts[*selected];
        for(const auto& split:splits)if(std::abs(split.first-best_t)<1e-9)return split.second;
        while(nodes.count(virtual_node))--virtual_node;
        const int id=virtual_node--;
        nodes[id]=nodes.at(edge.a)+(nodes.at(edge.b)-nodes.at(edge.a))*best_t;
        splits.push_back({best_t,id});return id;
    };
    const auto start_attachment=stand_attachment(start,tokens.front());
    const auto end_attachment=stand_attachment(end,tokens.back());
    using Arc=std::pair<int,double>;
    std::vector<std::map<int,std::vector<Arc>>> graphs(tokens.size());
    for(size_t stage=0;stage<tokens.size();++stage) {
        auto& graph=graphs[stage];
        for(size_t i=0;i<airport.edges.size();++i) {
            const auto& e=airport.edges[i];
            if(e.runway||route_key(e.name)!=tokens[stage])continue;
            auto a=airport.nodes.find(e.a),b=airport.nodes.find(e.b);
            if(a==airport.nodes.end()||b==airport.nodes.end())continue;
            double distance=length(b->second-a->second);if(distance<1e-6)continue;
            std::vector<std::pair<double,int>> splits{{0,e.a},{1,e.b}};
            if(cuts.count(i))splits.insert(splits.end(),cuts.at(i).begin(),cuts.at(i).end());
            std::sort(splits.begin(),splits.end());
            for(size_t j=1;j<splits.size();++j) {
                const int from=splits[j-1].second,to=splits[j].second;
                const double metres=length(nodes.at(to)-nodes.at(from));
                graph[from].push_back({to,metres});graph[to];
                if(!e.oneway)graph[to].push_back({from,metres});
            }
        }
        if(graph.empty())return fail("Unknown taxiway: "+tokens[stage]);
    }
    auto endpoint_nodes=[&](const Endpoint& ep,const std::map<int,std::vector<Arc>>& graph,std::optional<int> attachment) {
        std::set<int> candidates;
        if(ep.stand) {
            if(attachment)candidates.insert(*attachment);
        } else if(ep.runway) {
            // End at the runway boundary/centreline; do not invent a line to
            // the threshold or extend a taxi clearance along the runway.
            for(const auto& n:graph) {
                auto station=ep.runway->station(nodes.at(n.first));
                if(station.x>=-5&&station.x<=ep.runway->length()+5&&
                   std::abs(station.y)<=ep.runway->width*.5+5)candidates.insert(n.first);
            }
        } else for(const auto& n:graph)candidates.insert(n.first);
        return candidates;
    };
    auto starts=endpoint_nodes(start,graphs.front(),start_attachment),ends=endpoint_nodes(end,graphs.back(),end_attachment);
    if(starts.empty()||ends.empty())return fail("Specified taxiway does not reach the runway.");
    // used flag prevents accepting a taxiway name without traversing any of it.
    using State=std::tuple<int,int,bool>;
    using Item=std::pair<double,State>;
    std::map<State,double> distances;std::map<State,State> previous;
    std::priority_queue<Item,std::vector<Item>,std::greater<Item>> queue;
    for(int node:starts){State s{0,node,false};distances[s]=0;queue.push({0,s});}
    std::optional<State> goal;
    auto relax=[&](State from,State to,double distance) {
        auto found=distances.find(to);
        if(found==distances.end()||distance<found->second-1e-8) {
            distances[to]=distance;previous[to]=from;queue.push({distance,to});
        }
    };
    while(!queue.empty()) {
        auto item=queue.top();queue.pop();const auto state=item.second;
        if(item.first>distances.at(state)+1e-8)continue;
        auto [stage,node,used]=state;
        if(stage==int(tokens.size())-1&&used&&ends.count(node)){goal=state;break;}
        for(auto arc:graphs[stage].at(node))relax(state,{stage,arc.first,true},item.first+arc.second);
        if(used&&stage+1<int(tokens.size())&&graphs[stage+1].count(node))
            relax(state,{stage+1,node,false},item.first);
    }
    if(!goal)return fail("No connected route in this order (check one-way links).");
    for(State s=*goal;;s=previous.at(s)) {
        Vec2 p=nodes.at(std::get<1>(s));
        if(result.points.empty()||length(result.points.back()-p)>1e-6)result.points.push_back(p);
        if(!previous.count(s))break;
    }
    std::reverse(result.points.begin(),result.points.end());
    if(start.stand&&length(result.points.front()-start.stand->pos)>1e-6)
        result.points.insert(result.points.begin(),start.stand->pos);
    if(end.stand&&length(result.points.back()-end.stand->pos)>1e-6)result.points.push_back(end.stand->pos);
    for(size_t i=1;i<result.points.size();++i)result.metres+=length(result.points[i]-result.points[i-1]);
    return result;
}
inline constexpr double route_half_width_m=10;
// Mitered strip: +/-10 metres from the centreline. Limit sharp corner spikes.
inline std::vector<std::array<Vec2,4>> route_ribbon(const std::vector<Vec2>& points) {
    std::vector<std::array<Vec2,4>> quads;if(points.size()<2)return quads;
    std::vector<Vec2> sides;
    for(size_t i=0;i<points.size();++i) {
        auto normal=[](Vec2 d){double n=length(d);return n>1e-9?Vec2{-d.y/n,d.x/n}:Vec2{};};
        auto a=normal(points[i?i:1]-points[i?i-1:0]);
        auto b=i+1<points.size()?normal(points[i+1]-points[i]):a;
        auto sum=a+b;double n=length(sum);
        if(n<1e-6)sides.push_back(b*route_half_width_m);
        else {sum=sum*(1/n);sides.push_back(sum*std::min(2*route_half_width_m,route_half_width_m/std::max(.001,dot(sum,b))));}
    }
    for(size_t i=1;i<points.size();++i)quads.push_back({points[i-1]+sides[i-1],points[i-1]-sides[i-1],points[i]-sides[i],points[i]+sides[i]});
    return quads;
}
}
