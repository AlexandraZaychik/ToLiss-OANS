#include "nd_ui.hpp"
#include <algorithm>
#include <chrono>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <regex>

namespace oans {
namespace {
std::string upper(std::string s) {for(char& c:s)c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));return s;}
char initial_of(const std::string& s) {return !s.empty()&&s[0]>='A'&&s[0]<='Z'?s[0]:'#';}
constexpr int dropdown_rows=3;
const std::string initials="#ABCDEFGHIJKLMNOPQRSTUVWXYZ";
const Box initial_dropdown_box{108,0,158,78};
Box airport_dropdown_box(const UiState& s,double top=78) {
    // Real panel: ICAO/IATA share the compact width; CITY fits about 20 characters.
    return {170,0,s.airport_mode==2?398.0:294.0,top};
}
const Box feature_dropdown_box{286,0,400,78},feature_initial_dropdown_box{210,0,258,78};
const std::vector<MapFeature>* feature_items(const UiState& s,const UiModel& m) {
    return s.feature_kind>=0&&s.feature_kind<4?m.features[s.feature_kind]:nullptr;
}
void reveal_airport(UiState& s,int at) {
    s.airport_first=std::clamp(at,0,std::max(0,int(s.results.size())-dropdown_rows));
}
std::string coordinate(double value,bool longitude) {
    const int tenths=static_cast<int>(std::round(std::abs(value)*600));
    std::ostringstream out;out<<std::setfill('0')<<std::setw(longitude?3:2)<<tenths/600
        <<' '<<std::setw(2)<<(tenths%600)/10<<'.'<<tenths%10
        <<(longitude?(value<0?'W':'E'):(value<0?'S':'N'));
    return out.str();
}
}
double panel_height(Panel) {return 110;}
std::string airport_key(const IndexEntry& entry,int mode) {
    return upper(mode==1?entry.iata:mode==2?entry.name:entry.icao);
}
AirportDatabaseStatus read_airport_database_status(std::istream& input) {
    AirportDatabaseStatus result;
    const std::regex cycle(R"(^\s*AIRAC cycle\s*:\s*([0-9]{4})\s*$)");
    const std::regex validity(R"(^\s*Valid \(from/to\)\s*:\s*([0-9]{2})/([A-Za-z]{3})/([0-9]{4})\s*-\s*([0-9]{2})/([A-Za-z]{3})/([0-9]{4})\s*$)");
    auto date_valid=[](const std::string& day,const std::string& month,const std::string& year) {
        const std::string months="JANFEBMARAPRMAYJUNJULAUGSEPOCTNOVDEC";
        auto at=months.find(upper(month));
        if(at==std::string::npos||at%3)return false;
        const int days[]={31,28,31,30,31,30,31,31,30,31,30,31};
        const int y=std::stoi(year),d=std::stoi(day);
        const int max=days[at/3]+(at==3&&y%4==0&&(y%100!=0||y%400==0));
        return y>0&&d>0&&d<=max;
    };
    for(std::string row;std::getline(input,row);) {
        std::smatch match;
        if(std::regex_match(row,match,cycle))result.cycle=match[1];
        else if(std::regex_match(row,match,validity)&&date_valid(match[1],match[2],match[3])&&date_valid(match[4],match[5],match[6]))
            result.validity=match[1].str()+"/"+upper(match[2])+"-"+match[4].str()+"/"+upper(match[5]);
    }
    return result;
}
const UiControl* UiFrame::hit(Vec2 p) const {
    for(auto i=controls.rbegin();i!=controls.rend();++i)if(i->box.contains(p))return &*i;
    return nullptr;
}
UiFrame nd_controls(const UiState& s,const UiModel& m) {
    UiFrame f;
    auto button=[&](Box b,std::string label,UiAction a,int arg=0,bool enable=true,bool selected=false,bool pending=false,bool highlight=false) {f.controls.push_back({b,std::move(label),a,arg,enable,selected,pending,highlight});};
    auto text=[&](double x,double y,std::string label,int colour=0,double size=18,double width=-1) {f.texts.push_back({{x,y},std::move(label),colour,size,width});};
    auto dropdown=[&](Box b,int count,int first,UiAction scroll) {
        // Added after base controls so the menu owns drawing and hits below it.
        button(b,"",UiAction::dropdown_background);
        if(count<=dropdown_rows)return;
        const double top=b.t-2,bottom=b.b+2,track=top-bottom;
        const double height=std::max(10.0,track*dropdown_rows/count);
        const double thumb_top=top-(track-height)*first/(count-dropdown_rows);
        button({b.r-14,thumb_top,b.r-2,top},"",scroll,-dropdown_rows);
        button({b.r-14,bottom,b.r-2,thumb_top-height},"",scroll,dropdown_rows);
        button({b.r-14,thumb_top-height,b.r-2,thumb_top},"",UiAction::dropdown_thumb);
    };
    if(s.panel==Panel::closed) {
        if(!s.follow)button({8,8,174,37},"CENTER ACFT",UiAction::center);
        button({0,78,96,110},"^^",UiAction::panel_toggle);
    } else {
        f.status_style=true;
        // Reference: bottom-left vertical tabs and a compact content pane.
        const double height=panel_height(s.panel);
        f.backgrounds={{0,0,600,26},{96,0,600,height}};
        f.rules={{96,0,600,1},{96,height-1,600,height},{96,0,97,height},{599,0,600,height}};
        button({0,52,96,78},"MAP DATA",UiAction::map_data,0,true,s.panel==Panel::map_data);
        button({0,26,96,52},"ARPT SEL",UiAction::airport,0,true,s.panel==Panel::airport);
        button({0,0,96,26},"STATUS",UiAction::status,0,true,s.panel==Panel::status);
        button({0,78,96,110},"vv",UiAction::panel_toggle);
        if(s.panel==Panel::status) {
            f.rules.insert(f.rules.end(),{{110,57,586,58},{110,20,586,21}});
            text(114,76,"ACTIVE",0,16);
            text(292,76,m.database.validity.empty()?"UNAVAILABLE":m.database.validity,m.database.validity.empty()?1:2,16,274);
            text(114,34,"AIRPORT DATABASE",0,14);
            text(324,34,m.database.cycle.empty()?"UNAVAILABLE":"GATEWAY"+m.database.cycle,m.database.cycle.empty()?1:2,16,262);
        } else if(s.panel==Panel::airport) {
            const IndexEntry* sel=s.selection>=0&&s.selection<int(s.results.size())?&s.results[s.selection]:nullptr;
            const auto* details=s.confirmed_airport?&*s.confirmed_airport:sel;
            const bool pending=s.airport_modifying;
            const bool initial_focus=s.initial_dropdown||(!s.airport_dropdown&&s.airport_initial_focus);
            const auto key=sel?airport_key(*sel,s.airport_mode):"----";
            button({108,78,158,104},s.airport_initials.empty()?"-":std::string(1,s.initial),UiAction::initial_list,0,!s.airport_initials.empty(),s.initial_dropdown,pending,pending&&initial_focus);
            button({170,78,268,104},s.editing&&!s.query.empty()?s.query+"_":key,UiAction::focus,0,true,false,pending,pending&&!initial_focus);
            button({268,78,294,104},"v",UiAction::airport_list,0,true,s.airport_dropdown);
            static const char* modes[]={"ICAO","IATA","CITY NAME"};
            for(int i=0;i<3;++i)button({108,50-i*24.0,282,74-i*24.0},modes[i],UiAction::airport_mode,i,true,s.airport_mode==i);
            f.rules.push_back({498,4,499,104});
            const std::string codes[]={m.fms_origin,m.fms_destination,m.fms_alternate};
            const char* roles[]={"ORIG","DEST","ALTN"};
            for(int i=0;i<3;++i)button({510,78-i*36.0,590,104-i*36.0},codes[i].empty()?roles[i]:codes[i],UiAction::fms_airport,i,!codes[i].empty());
            if(details) {
                std::string name=upper(details->name),first=name.substr(0,20),second;
                if(name.size()>20) {
                    const auto split=first.find_last_of(' ');
                    if(split!=std::string::npos&&split>8) {first=name.substr(0,split);second=name.substr(split+1);}
                    else second=name.substr(20);
                }
                const int colour=pending?3:2;
                text(304,92,first,colour,11,184);text(304,78,second,colour,11,184);
                text(304,58,details->icao+"  "+(details->iata.empty()?"---":details->iata),colour,12,184);
                text(304,40,coordinate(details->lat,false)+"/"+coordinate(details->lon,true),colour,10,184);
            } else text(304,64,m.index_ready?"NO MATCH":"INDEXING...",1,12,184);
            button({310,4,486,28},"DISPLAY ARPT",UiAction::display_airport,0,sel!=nullptr);
            if(s.initial_dropdown) {
                const auto& initials=s.airport_initials;
                const int first=std::clamp(s.initial_first,0,std::max(0,int(initials.size())-dropdown_rows));
                dropdown(initial_dropdown_box,int(initials.size()),first,UiAction::initial_scroll);
                for(int row=0;row<dropdown_rows&&first+row<int(initials.size());++row) {
                    const int i=first+row;
                    button({110,52-row*25.0,142,77-row*25.0},initials.substr(i,1),UiAction::initial_select,initials[i],true,s.initial==initials[i]);
                }
            }
            if(s.airport_dropdown) {
                const int first=std::clamp(s.airport_first,0,std::max(0,int(s.results.size())-dropdown_rows));
                const auto box=airport_dropdown_box(s);
                dropdown(box,int(s.results.size()),first,UiAction::airport_scroll);
                for(int row=0;row<dropdown_rows&&first+row<int(s.results.size());++row) {
                    const int i=first+row;
                    button({box.l+2,52-row*25.0,box.r-16,77-row*25.0},airport_key(s.results[i],s.airport_mode),UiAction::result,i,true,s.selection==i);
                }
            }
        } else {
            static const char* kinds[]={"RWY","TWY","STAND","OTHER"};
            const auto* items=feature_items(s,m);
            const MapFeature* selected=items&&s.feature_selection>=0&&s.feature_selection<int(items->size())?&(*items)[s.feature_selection]:nullptr;
            const bool available=items&&!items->empty();
            const bool initial_enabled=available&&(s.feature_kind>=1&&s.feature_kind<=3);
            const bool pending=available&&(s.feature_initial_dropdown||s.feature_dropdown);
            for(int i=0;i<4;++i)button({108,74-i*24.0,204,98-i*24.0},kinds[i],UiAction::feature_kind,i,true,s.feature_kind==i);
            button({210,78,258,104},initial_enabled?std::string(1,s.feature_initial):"-",UiAction::feature_initial_list,0,initial_enabled,s.feature_initial_dropdown,pending,pending&&initial_enabled&&s.feature_initial_dropdown);
            button({286,78,400,104},selected?selected->name:"---",UiAction::feature_list,0,available,s.feature_dropdown,pending,pending&&s.feature_dropdown);
            button({430,58,498,102},"ADD\nCROSS",UiAction::add_cross,0,selected!=nullptr);
            button({506,58,574,102},"ADD\nFLAG",UiAction::add_flag,0,selected!=nullptr);
            button({430,8,498,52},"LDG\nSHIFT",UiAction::landing_shift,0,false);
            button({506,8,574,52},"CENTER\nMAP",UiAction::feature_center,0,selected!=nullptr);
            button({578,84,598,106},"X",UiAction::close);
            if(s.feature_initial_dropdown&&initial_enabled) {
                const auto initials=map_feature_initials(s,m);
                const int first=std::clamp(s.feature_initial_first,0,std::max(0,int(initials.size())-dropdown_rows));
                dropdown(feature_initial_dropdown_box,int(initials.size()),first,UiAction::feature_initial_scroll);
                for(int row=0;row<dropdown_rows&&first+row<int(initials.size());++row) {
                    const int i=first+row;
                    button({212,52-row*25.0,242,77-row*25.0},initials.substr(i,1),UiAction::feature_initial_select,initials[i],true,s.feature_initial==initials[i]);
                }
            }
            if(s.feature_dropdown&&available) {
                const int first=std::clamp(s.feature_first,0,std::max(0,int(items->size())-dropdown_rows));
                dropdown(feature_dropdown_box,int(items->size()),first,UiAction::feature_scroll);
                for(int row=0;row<dropdown_rows&&first+row<int(items->size());++row) {
                    const int i=first+row;
                    button({288,52-row*25.0,384,77-row*25.0},(*items)[i].name,UiAction::feature_select,i,true,s.feature_selection==i);
                }
            }
        }
    }
    // Reserve the optional bottom status row without changing map projection.
    const auto lift=[&](Box& b) {b.b+=m.bottom_inset;b.t+=m.bottom_inset;};
    for(auto& b:f.backgrounds)lift(b);
    for(auto& b:f.rules)lift(b);
    for(auto& c:f.controls) {
        if(m.bottom_inset>0&&c.action==UiAction::panel_toggle)
            c.box={0,stus_bar_safe_bottom,96,m.bottom_inset-1};
        else lift(c.box);
    }
    for(auto& t:f.texts)t.at.y+=m.bottom_inset;
    if(s.menu) {
        const double x=s.menu_at.x,top=s.menu_at.y;
        f.backgrounds.push_back({x,top-6*32,x+210,top});
        const std::pair<const char*,UiAction> items[]={{"ADD CROSS",UiAction::add_cross},{"ADD FLAG",UiAction::add_flag},{"MENU",UiAction::map_data},{"ERASE CROSSES",UiAction::erase_crosses},{"ERASE FLAGS",UiAction::erase_flags},{"CENTER ON ACFT",UiAction::center}};
        for(int i=0;i<6;++i) {
            const auto action=items[i].second;
            const bool enabled=action==UiAction::erase_crosses?!s.crosses.empty():action==UiAction::erase_flags?!s.flags.empty():true;
            button({x+3,top-(i+1)*32+2,x+207,top-i*32-2},items[i].first,action,0,enabled);
        }
    }
    return f;
}

void show_context(UiState& s,Vec2 at,Vec2 world) {
    const double lowest_top=(s.panel==Panel::closed?55.0:panel_height(s.panel)+12)+6*32;
    s.menu=true;s.editing=false;s.menu_at={std::clamp(at.x,8.0,382.0),std::clamp(at.y,lowest_top,548.0)};s.context_world=world;
}
bool zoom(UiState& s,bool closer) {
    static constexpr double ranges[]={5,2,1,.5,.2};int i=0;
    while(i<4&&std::abs(s.range_nm-ranges[i])>.001)++i;
    int j=std::clamp(i+(closer?1:-1),0,4);s.range_nm=ranges[j];return i!=j;
}
void search_airports(UiState& s,const std::vector<IndexEntry>& index,double,double) {
    const auto previous=s.selection>=0&&s.selection<int(s.results.size())?s.results[s.selection].icao:"";
    s.airport_first=0;s.selection=-1;s.results.clear();s.airport_initials.clear();s.initial_first=0;s.wheel={};const auto q=upper(s.query);
    if(!q.empty())s.initial=initial_of(q);
    std::vector<std::pair<std::string,const IndexEntry*>> matches;
    for(const auto& a:index) {
        if(!std::isfinite(a.lat)||!std::isfinite(a.lon))continue;
        auto key=airport_key(a,s.airport_mode);
        if(key.empty())continue;
        matches.push_back({std::move(key),&a});
    }
    std::sort(matches.begin(),matches.end(),[](const auto& a,const auto& b){return a.first==b.first?a.second->icao<b.second->icao:a.first<b.first;});
    for(const auto& match:matches) {
        if(match.second->icao==previous)s.selection=static_cast<int>(s.results.size());
        s.results.push_back(*match.second);
    }
    bool present[27]={};
    for(const auto& match:matches)present[initials.find(initial_of(match.first))]=true;
    for(size_t i=0;i<initials.size();++i)if(present[i])s.airport_initials+=initials[i];
    if(!q.empty()) {
        s.selection=-1;
        for(int i=0;i<int(s.results.size());++i)if(airport_key(s.results[i],s.airport_mode).rfind(q,0)==0) {s.selection=i;break;}
    } else if(s.selection<0) {
        locate_airport_initial(s,s.initial);return;
    }
    if(s.selection>=0)reveal_airport(s,s.selection);
}
void select_airport_candidate(UiState& s,const std::vector<IndexEntry>& index,const std::string& icao) {
    auto entry=find_airport(index,icao);
    if(!entry||airport_key(*entry,s.airport_mode).empty())return;
    s.query.clear();s.initial=initial_of(airport_key(*entry,s.airport_mode));
    search_airports(s,index,0,0);
    for(size_t i=0;i<s.results.size();++i)if(s.results[i].icao==entry->icao) {s.selection=static_cast<int>(i);reveal_airport(s,s.selection);break;}
}
void begin_airport_modification(UiState& s,bool initial_focus) {
    if(!s.confirmed_airport&&s.selection>=0&&s.selection<int(s.results.size()))s.confirmed_airport=s.results[s.selection];
    s.airport_modifying=true;s.airport_initial_focus=initial_focus;
}
bool confirm_airport_candidate(UiState& s) {
    if(s.selection<0||s.selection>=int(s.results.size()))return false;
    s.confirmed_airport=s.results[s.selection];s.airport_modifying=false;s.airport_initial_focus=false;
    s.initial=initial_of(airport_key(*s.confirmed_airport,s.airport_mode));s.query.clear();
    return true;
}
void locate_airport_initial(UiState& s,char initial) {
    if(initials.find(initial)==std::string::npos)return;
    s.initial=initial;s.query.clear();s.selection=-1;
    int at=0;
    // Missing initials jump to the next available letter, or the end. They
    // never remove entries from the scrollable list.
    for(;at<int(s.results.size());++at) {
        const auto c=initial_of(airport_key(s.results[at],s.airport_mode));
        if(c==initial||(initial!='#'&&c>initial))break;
    }
    if(!s.results.empty())s.selection=std::min(at,int(s.results.size())-1);
    reveal_airport(s,at);
}
void scroll_airports(UiState& s,int rows) {
    const auto target=static_cast<long long>(s.airport_first)+rows;
    s.airport_first=static_cast<int>(std::clamp(target,0LL,static_cast<long long>(std::max(0,int(s.results.size())-dropdown_rows))));
}
bool scroll_airport_dropdown(UiState& s,Vec2 point,int clicks) {
    if(s.panel!=Panel::airport||s.menu)return false;
    if(s.airport_dropdown&&airport_dropdown_box(s,104).contains(point)) {
        scroll_airports(s,-std::clamp(clicks,-100,100));return true;
    }
    if(s.initial_dropdown&&Box{108,0,158,104}.contains(point)) {
        scroll_initials(s,UiModel{},false,-std::clamp(clicks,-100,100));return true;
    }
    return false;
}
void scroll_map_features(UiState& s,const UiModel& m,int rows) {
    const auto* items=feature_items(s,m);
    const auto target=static_cast<long long>(s.feature_first)+rows;
    s.feature_first=static_cast<int>(std::clamp(target,0LL,static_cast<long long>(std::max(0,(items?int(items->size()):0)-dropdown_rows))));
}
void locate_map_feature_initial(UiState& s,const UiModel& m,char initial) {
    const auto* items=feature_items(s,m);
    if(s.feature_kind==0||!items||items->empty()||initials.find(initial)==std::string::npos)return;
    s.feature_initial=initial;
    int at=0;
    for(;at<int(items->size());++at) {
        const auto c=initial_of(upper((*items)[at].name));
        if(c==initial||(initial!='#'&&c>initial))break;
    }
    s.feature_selection=std::min(at,int(items->size())-1);
    s.context_world=(*items)[s.feature_selection].pos;
    s.feature_first=s.feature_selection;scroll_map_features(s,m,0);
    s.feature_initial_dropdown=false;s.feature_dropdown=true;
}
std::string map_feature_initials(const UiState& s,const UiModel& m) {
    const auto* items=feature_items(s,m);
    if(s.feature_kind==0||!items)return {};
    bool present[27]={};
    for(const auto& item:*items)present[initials.find(initial_of(upper(item.name)))]=true;
    std::string result;
    for(size_t i=0;i<initials.size();++i)if(present[i])result+=initials[i];
    return result;
}
void scroll_initials(UiState& s,const UiModel& m,bool feature,int rows) {
    const auto letters=feature?map_feature_initials(s,m):s.airport_initials;
    auto& first=feature?s.feature_initial_first:s.initial_first;
    first=static_cast<int>(std::clamp(static_cast<long long>(first)+rows,0LL,
        static_cast<long long>(std::max(0,int(letters.size())-dropdown_rows))));
}
int UiState::WheelAcceleration::apply(int clicks,int list,double now) {
    if(!clicks)return 0;
    const int sign=clicks>0?1:-1;
    if(last<0||now<last||now-last>0.4||direction!=sign||target!=list)started=now;
    last=now;direction=sign;target=list;
    const int multiplier=1+static_cast<int>(std::min(15.0,std::max(0.0,now-started)/0.3));
    return std::clamp(clicks,-100,100)*multiplier;
}
bool scroll_nd_dropdown(UiState& s,const UiModel& m,Vec2 point,int clicks,double now) {
    int target=0;
    if(!s.menu&&s.panel==Panel::airport) {
        if(s.airport_dropdown&&airport_dropdown_box(s,104).contains(point))target=1;
        else if(s.initial_dropdown&&Box{108,0,158,104}.contains(point))target=2;
    } else if(!s.menu&&s.panel==Panel::map_data&&feature_items(s,m)) {
        if(s.feature_dropdown&&Box{286,0,400,104}.contains(point))target=3+s.feature_kind*2;
        else if(s.feature_initial_dropdown&&s.feature_kind!=0&&Box{210,0,258,104}.contains(point))target=4+s.feature_kind*2;
    }
    if(!target) {s.wheel={};return false;}
    if(now<0)now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const int rows=-s.wheel.apply(clicks,target,now);
    if(target==1)scroll_airports(s,rows);
    else if(target==2)scroll_initials(s,m,false,rows);
    else if(target%2)scroll_map_features(s,m,rows);
    else scroll_initials(s,m,true,rows);
    return true;
}
void Gesture::begin(Vec2 p,Vec2 pixels,bool map_area) {pressed=true;moved=false;map=map_area;down=last=p;screen=pixels;}
bool Gesture::move(Vec2 pixels) {if(pressed&&length(pixels-screen)>8)moved=true;return pressed&&moved&&map;}
}
