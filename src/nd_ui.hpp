#pragma once
// Native ND controls, visually adapted from the A350 OANS reference.
#include "core.hpp"
#include <istream>

namespace oans {
inline constexpr double stus_bar_height=32,stus_bar_safe_bottom=8;
enum class Panel { closed, map_data, airport, status };
enum class UiAction {
    none, map_data, airport, status, close, focus, clear, character, backspace,
    result, previous_page, next_page, display_airport, nearest, runway_previous,
    runway_next, zoom_in, zoom_out, center,
    add_cross, add_flag, erase_crosses, erase_flags, orient, report,
    panel_toggle, keyboard_toggle, feature_kind, feature_list, feature_select, feature_center,
    feature_previous, feature_next, fms_airport, airport_mode, initial_list,
    initial_select, initial_previous, initial_next, airport_list,
    dropdown_background, dropdown_thumb, airport_scroll, initial_scroll,
    feature_initial_list, feature_initial_select, feature_initial_scroll, feature_scroll, landing_shift
};
struct Box {
    double l=0,b=0,r=0,t=0;
    bool contains(Vec2 p) const {return p.x>=l&&p.x<r&&p.y>=b&&p.y<t;}
};
struct UiControl {Box box;std::string label;UiAction action=UiAction::none;int argument=0;bool enabled=true,selected=false,pending=false,highlight=false;};
struct UiText {Vec2 at;std::string text;int colour=0;double size=18,width=-1;};
struct MapFeature {std::string name;Vec2 pos;};
struct UiFrame {
    bool status_style=false;
    std::vector<Box> backgrounds;
    std::vector<Box> rules;
    std::vector<UiControl> controls;
    std::vector<UiText> texts;
    const UiControl* hit(Vec2 p) const;
};
struct AirportDatabaseStatus {
    std::string cycle,validity;
};
AirportDatabaseStatus read_airport_database_status(std::istream& input);
struct UiState {
    struct WheelAcceleration {
        double started=0,last=-1;
        int direction=0,target=0;
        int apply(int clicks,int list,double now);
    } wheel;
    Panel panel=Panel::closed;
    Panel last_panel=Panel::map_data;
    bool menu=false,editing=false,follow=true;
    Vec2 menu_at,context_world,center;
    double range_nm=1;
    // -1 follows the physical EFIS mode; 0 north up; 1 heading up.
    int orientation=-1;
    std::string query;
    std::vector<IndexEntry> results;
    std::string airport_initials;
    int airport_first=0,selection=-1;
    // Browsing is a draft; selecting a row confirms details, DISPLAY ARPT moves the map.
    std::optional<IndexEntry> confirmed_airport;
    bool airport_modifying=false;
    bool airport_initial_focus=false;
    std::vector<Vec2> crosses,flags;
    bool keyboard=false,feature_dropdown=false;
    int airport_mode=0; // ICAO, IATA, CITY NAME
    char initial='A';
    bool initial_dropdown=false,airport_dropdown=false;
    int initial_first=0;
    int feature_kind=0,feature_selection=-1,feature_first=0;
    char feature_initial='-';
    bool feature_initial_dropdown=false;
    int feature_initial_first=0;
};
struct UiModel {
    double bottom_inset=0;
    AirportDatabaseStatus database;
    std::string code="----",name,runway="--",status;
    std::string fms_origin,fms_destination,fms_alternate,fms_status="NOT CONNECTED";
    bool index_ready=false,profile=false,manual=false;
    int runways=0;
    int airports=0,pavements=0,stands=0,taxiways=0,terminals=0,buildings=0;
    const std::vector<MapFeature>* features[4]={nullptr,nullptr,nullptr,nullptr};
};
UiFrame nd_controls(const UiState& s,const UiModel& m);
void show_context(UiState& s,Vec2 at,Vec2 world);
bool zoom(UiState& s,bool closer);
// Reused by ND and diagnostic popup. One search per edit/open, never per draw.
void search_airports(UiState& s,const std::vector<IndexEntry>& index,double lat,double lon);
std::string airport_key(const IndexEntry& entry,int mode);
void select_airport_candidate(UiState& s,const std::vector<IndexEntry>& index,const std::string& icao);
void begin_airport_modification(UiState& s,bool initial_focus=false);
bool confirm_airport_candidate(UiState& s);
// Initials and typed prefixes locate entries in one complete, sorted list.
void locate_airport_initial(UiState& s,char initial);
void scroll_airports(UiState& s,int rows);
bool scroll_airport_dropdown(UiState& s,Vec2 point,int clicks);
void scroll_map_features(UiState& s,const UiModel& m,int rows);
void locate_map_feature_initial(UiState& s,const UiModel& m,char initial);
std::string map_feature_initials(const UiState& s,const UiModel& m);
void scroll_initials(UiState& s,const UiModel& m,bool feature,int rows);
bool scroll_nd_dropdown(UiState& s,const UiModel& m,Vec2 point,int clicks,double now=-1);
double panel_height(Panel panel);
struct Gesture {
    bool pressed=false,moved=false,map=false;
    Vec2 down,last,screen;
    void begin(Vec2 p,Vec2 pixels,bool map_area);
    bool move(Vec2 pixels);
    void clear() {pressed=false;moved=false;}
};
}
