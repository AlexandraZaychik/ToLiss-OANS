#include "nd_ui.hpp"
#include "map_labels.hpp"
#include "map_viewport.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <limits>
#include <sstream>
using namespace oans;
int checks=0;
void check(bool ok,const char* why) {++checks;if(!ok)throw std::runtime_error(why);}
const UiControl& control(const UiFrame& f,UiAction a) {for(const auto& c:f.controls)if(c.action==a)return c;throw std::runtime_error("missing control");}
void tests() {
    for(const auto panel:{Panel::closed,Panel::map_data,Panel::airport,Panel::status}) {
        UiState s;s.panel=panel;s.follow=false;
        UiModel m;const auto original=nd_controls(s,m);m.bottom_inset=32;
        const auto lifted=nd_controls(s,m);
        check(original.controls.size()==lifted.controls.size(),"status row retains panel controls");
        for(size_t i=0;i<original.controls.size();++i) {
            const auto& a=original.controls[i];const auto& b=lifted.controls[i];
            if(a.action==UiAction::panel_toggle) {
                check(b.box.l==0&&b.box.r==96&&b.box.b==8&&b.box.t==31,"panel arrow is inside left side of status row above bottom crop");
                check(b.label==a.label,"status row preserves expanded and collapsed arrow direction");
                const auto* at=lifted.hit({48,19});
                check(at&&at->action==UiAction::panel_toggle,"status row arrow hit target matches displayed location");
                check(!lifted.hit({48,4}),"bottom safety margin does not activate arrow");
                continue;
            }
            check(b.box.b==a.box.b+32&&b.box.t==a.box.t+32&&b.box.l==a.box.l,"controls lifted above status row");
            check(!lifted.hit({180,19}),"status row outside arrow cannot activate a hidden control");
            const Vec2 point{(b.box.l+b.box.r)*.5,(b.box.b+b.box.t)*.5};
            const auto* before=original.hit({point.x,point.y-32});const auto* after=lifted.hit(point);
            check(before&&after&&before->action==after->action,"lifted control picking matches displayed action");
        }
    }
    std::istringstream cycle("AIRAC cycle    : 2610\r\nVersion : 1\r\nValid (from/to): 01/OCT/2026 - 29/OCT/2026\r\n");
    const auto database=read_airport_database_status(cycle);
    check(database.cycle=="2610"&&database.validity=="01/OCT-29/OCT","read cycle and validity from X-Plane cycle_info format");
    std::istringstream missing;
    check(read_airport_database_status(missing).cycle.empty(),"missing cycle does not invent a database version");
    std::istringstream malformed("AIRAC cycle : 26X0\nValid (from/to): 31/FEB/2026 - 29/OCT/2026\n");
    const auto invalid=read_airport_database_status(malformed);
    check(invalid.cycle.empty()&&invalid.validity.empty(),"malformed cycle and impossible dates remain unavailable");
    UiState status_page;status_page.panel=Panel::status;
    UiModel status_model;status_model.database=database;
    const auto status_frame=nd_controls(status_page,status_model);
    check(status_frame.status_style&&status_frame.controls.size()==4,"single-database status has three vertical tabs and menu toggle, no swap or second database");
    check(control(status_frame,UiAction::panel_toggle).box.l==0&&control(status_frame,UiAction::panel_toggle).box.b==78,"STATUS collapse control sits immediately above bottom-left tabs");
    check(control(status_frame,UiAction::status).box.b==0&&control(status_frame,UiAction::map_data).box.l==0,"status menu touches the bottom left edge");
    for(const auto& c:status_frame.controls) {
        auto hit=status_frame.hit({(c.box.l+c.box.r)/2,(c.box.b+c.box.t)/2});
        check(hit&&hit->action==c.action,"status tabs and close have matching hit targets");
    }
    bool date=false,identifier=false;
    for(const auto& t:status_frame.texts) {date|=t.text=="01/OCT-29/OCT"&&t.colour==2;identifier|=t.text=="GATEWAY2610"&&t.colour==2;}
    check(date&&identifier,"status shows green validity and user-requested Gateway identifier");
    {
        UiModel city_model;
        UiState city;city.panel=Panel::airport;city.airport_dropdown=true;city.results.resize(8);
        for(int mode=0;mode<3;++mode) {
            city.airport_mode=mode;city.airport_first=0;
            const auto frame=nd_controls(city,city_model);
            const auto* row=frame.hit({350,65});
            check(mode==2?(row&&row->action==UiAction::result):(!row||row->action!=UiAction::result),"only CITY menu extends into the airport details region");
            check(scroll_airport_dropdown(city,{350,40},-1)==(mode==2),"CITY extension uses matching legacy wheel region");
            city.wheel={};
            check(scroll_nd_dropdown(city,city_model,{350,40},-1,1)==(mode==2),"CITY extension uses matching accelerated wheel region");
        }
    }
    for(int mode:{2,3,4}) {
        const auto viewport=map_viewport(mode,true,110);
        check(viewport.bottom==112&&viewport.anchor.y>viewport.bottom+20&&viewport.anchor.y+viewport.radius<552,"status uses compact viewport above its panel");
    }
    // All three reference panels now share the same compact viewport.
    for(int mode:{2,3,4}) {
        auto viewport=map_viewport(mode,true,panel_height(Panel::map_data));
        check(viewport.anchor.y>panel_height(Panel::map_data)+20,"open panel leaves centred feature visible");
        check(viewport.anchor.y+viewport.radius<552,"open-panel compass fits below header");
        MapProjection projection;projection.center={1240,-830};projection.anchor=viewport.anchor;
        projection.scale=viewport.radius/(.2*1852);projection.angle=250.7747*pi/180;
        auto mark=projection.center+Vec2{15,-21};
        check(length(projection.from(projection.to(mark))-mark)<1e-8,"panel viewport retains exact geographic mouse mapping");
    }
    UiState s;UiModel m;
    auto f=nd_controls(s,m);check(f.controls.size()==1&&f.controls[0].action==UiAction::panel_toggle,"closed map leaves only the panel control while following aircraft");
    check(f.controls[0].box.l==0&&f.controls[0].box.b==78,"closed menu opens from same bottom-left location");
    s.panel=Panel::map_data;f=nd_controls(s,m);
    check(f.status_style&&panel_height(s.panel)==110&&control(f,UiAction::map_data).selected,"MAP DATA uses the shared compact panel and selected vertical tab");
    check(!control(f,UiAction::feature_initial_list).enabled,"RWY left selector unavailable");
    check(!control(f,UiAction::landing_shift).enabled,"LDG SHIFT always unavailable");
    check(!control(f,UiAction::feature_center).enabled,"map element center unavailable without a selection");
    check(!control(f,UiAction::add_cross).enabled,"MAP DATA cannot place a mark without a geographic element");
    std::vector<MapFeature> runways={{"17",{20,30}}};m.features[0]=&runways;s.feature_selection=0;
    f=nd_controls(s,m);check(control(f,UiAction::feature_center).enabled&&control(f,UiAction::add_cross).enabled,"selected runway enables centering and geographic marks");
    check(control(f,UiAction::feature_list).enabled&&!control(f,UiAction::feature_initial_list).enabled,"RWY right selector works while left stays unavailable");
    s.feature_dropdown=true;f=nd_controls(s,m);
    check(control(f,UiAction::feature_list).highlight&&!control(f,UiAction::feature_initial_list).highlight,"RWY value can receive focus without highlighting disabled initial");
    s.feature_dropdown=false;
    int kinds=0;for(const auto& c:f.controls)if(c.action==UiAction::feature_kind)++kinds;
    check(kinds==4,"four reference modes RWY TWY STAND OTHER");
    check(control(f,UiAction::add_cross).label=="ADD\nCROSS"&&control(f,UiAction::feature_center).label=="CENTER\nMAP","reference action labels use two lines");
    std::vector<MapFeature> features;
    for(char letter:{'A','B','C'})for(int i=0;i<5;++i)features.push_back({std::string(1,letter)+std::to_string(i),{double(i),double(letter)}});
    m.features[1]=m.features[2]=&features;
    for(int kind:{1,2}) {
        s.feature_kind=kind;s.feature_selection=-1;f=nd_controls(s,m);
        check(control(f,UiAction::feature_initial_list).enabled&&control(f,UiAction::feature_list).enabled,"TWY and STAND enable both selectors when data is available");
        locate_map_feature_initial(s,m,'B');
        check(s.feature_selection==5&&s.feature_first==5&&s.feature_dropdown&&s.context_world.y=='B',"feature initial locates B and opens full list with correct geographic point");
        check(scroll_nd_dropdown(s,m,{320,39},1)&&s.feature_first==4,"feature wheel crosses backward from B to A");
        check(scroll_nd_dropdown(s,m,{320,39},-6)&&s.feature_first==10,"feature wheel crosses forward from B to C");
        scroll_map_features(s,m,1000);check(s.feature_first==12,"feature list clamps at final three rows");
        f=nd_controls(s,m);check(control(f,UiAction::feature_select).box.t<=78,"map feature dropdown opens downward");
        check(control(f,UiAction::feature_list).highlight&&control(f,UiAction::feature_initial_list).pending&&!control(f,UiAction::feature_initial_list).highlight,"TWY and STAND right dropdown highlights value and leaves initial yellow");
        s.feature_dropdown=false;s.feature_initial_dropdown=true;s.feature_initial_first=0;
        check(scroll_nd_dropdown(s,m,{225,39},-100)&&s.feature_initial_first==0,"three available feature initials need no scrolling");
        f=nd_controls(s,m);
        check(control(f,UiAction::feature_initial_list).highlight&&control(f,UiAction::feature_list).pending&&!control(f,UiAction::feature_list).highlight,"TWY and STAND left dropdown highlights initial and leaves value yellow");
        for(const auto& c:f.controls)check(c.box.l>=0&&c.box.b>=0&&c.box.r<=600&&c.box.t<=110,"compact MAP DATA controls fit panel");
        s.feature_initial_dropdown=false;
        f=nd_controls(s,m);
        check(!control(f,UiAction::feature_list).pending&&!control(f,UiAction::feature_initial_list).highlight,"closed feature selectors return to normal presentation");
    }
    s.feature_kind=3;s.feature_selection=0;s.feature_dropdown=s.feature_initial_dropdown=true;f=nd_controls(s,m);
    check(!control(f,UiAction::feature_initial_list).enabled&&!control(f,UiAction::feature_list).enabled,"OTHER disables both selectors even after previous feature selection");
    check(!control(f,UiAction::add_cross).enabled&&!control(f,UiAction::add_flag).enabled&&!control(f,UiAction::feature_center).enabled,"OTHER has no stale selected-feature actions");
    check(!scroll_nd_dropdown(s,m,{320,39},1),"OTHER cannot scroll stale feature dropdown");
    check(!control(f,UiAction::landing_shift).enabled,"LDG SHIFT stays unavailable in OTHER mode");
    std::vector<MapFeature> other_features={{"TWR",{321,654}}};m.features[3]=&other_features;
    s.feature_initial_dropdown=true;f=nd_controls(s,m);
    check(control(f,UiAction::feature_initial_list).enabled&&control(f,UiAction::feature_initial_select).label=="T","OTHER exposes only T when TWR is available");
    locate_map_feature_initial(s,m,'T');f=nd_controls(s,m);
    check(s.feature_selection==0&&s.context_world.x==321&&s.context_world.y==654&&s.feature_dropdown,"T locator selects exact tower position and opens right dropdown");
    check(control(f,UiAction::feature_select).label=="TWR"&&control(f,UiAction::add_cross).enabled&&control(f,UiAction::add_flag).enabled&&control(f,UiAction::feature_center).enabled,"TWR offers geographic marks and CENTER MAP");
    check(scroll_nd_dropdown(s,m,{320,39},-100)&&s.feature_first==0,"single TWR dropdown consumes wheel without moving or zooming");
    other_features.clear();f=nd_controls(s,m);
    check(!control(f,UiAction::feature_list).enabled&&!control(f,UiAction::feature_center).enabled&&!control(f,UiAction::add_flag).enabled,"missing tower disables stale OTHER selection and actions");
    m.features[3]=nullptr;
    s.feature_kind=0;s.feature_selection=0;s.feature_dropdown=s.feature_initial_dropdown=false;
    std::vector<IndexEntry> index;
    for(int i=0;i<120;++i) {IndexEntry e;e.icao="Z"+std::to_string(1000+i);e.name="Test Airport "+std::to_string(i);e.lat=30+i*.01;e.lon=120;index.push_back(e);}
    IndexEntry a;a.icao="ZSQD";a.header_id="XZ001I";a.icao_explicit=true;a.name="Qingdao Jiaodong";a.lat=36;a.lon=120;index.push_back(a);
    a.city="Qingdao";index.back()=a;
    IndexEntry bom;bom.icao="VABB";bom.iata="BOM";bom.city="Mumbai";bom.name="Chhatrapati Shivaji";bom.lat=19.0917;bom.lon=72.8667;
    IndexEntry aa=bom;aa.icao="VAAA";aa.iata="AAA";aa.city="A City";
    IndexEntry cc=bom;cc.icao="VCCC";cc.iata="CCC";cc.city="C City";
    IndexEntry missing_codes=bom;missing_codes.icao="VNUL";missing_codes.iata.clear();missing_codes.city.clear();
    index.insert(index.end(),{cc,bom,missing_codes,aa});
    {
        IndexEntry cairo=bom;cairo.icao="HECA";cairo.iata="CAI";cairo.city="Cairo";cairo.name="Cairo Intl";
        IndexEntry hongkong=bom;hongkong.icao="VHHH";hongkong.iata="HKG";hongkong.city="Hong Kong";hongkong.name="Hong Kong Intl";
        std::vector<IndexEntry> sample={cairo,hongkong};UiState draft;draft.panel=Panel::airport;
        select_airport_candidate(draft,sample,"HECA");check(confirm_airport_candidate(draft),"initial airport confirms successfully");
        auto normal=nd_controls(draft,m);
        check(!control(normal,UiAction::initial_list).pending&&!control(normal,UiAction::focus).highlight,"confirmed airport has normal green selector without blue editing highlight");
        begin_airport_modification(draft);select_airport_candidate(draft,sample,"VHHH");draft.initial='H';draft.airport_dropdown=true;
        auto pending=nd_controls(draft,m);
        check(draft.confirmed_airport->icao=="HECA"&&control(pending,UiAction::focus).label=="VHHH","candidate field differs from previously confirmed airport details");
        check(control(pending,UiAction::initial_list).label=="H"&&control(pending,UiAction::initial_list).pending,"initial H uses pending yellow presentation");
        check(control(pending,UiAction::focus).highlight,"candidate field has dark text on cyan selection highlight");
        for(int mode=0;mode<3;++mode) {
            draft.airport_mode=mode;draft.airport_dropdown=false;draft.initial_dropdown=true;
            begin_airport_modification(draft,true);
            auto left=nd_controls(draft,m);
            check(control(left,UiAction::initial_list).highlight&&control(left,UiAction::focus).pending&&!control(left,UiAction::focus).highlight,"ICAO IATA and city left focus highlights initial and leaves value yellow");
            draft.initial_dropdown=false;
            check(control(nd_controls(draft,m),UiAction::initial_list).highlight,"closing left list retains last airport editing focus until confirmation");
            begin_airport_modification(draft);draft.airport_dropdown=true;
            auto right=nd_controls(draft,m);
            check(!control(right,UiAction::initial_list).highlight&&control(right,UiAction::initial_list).pending&&control(right,UiAction::focus).highlight,"switching to airport value reverses selector colours");
        }
        draft.airport_mode=0;
        bool old_name=false,old_codes=false,new_name=false;
        for(const auto& t:pending.texts) {
            old_name|=t.text=="CAIRO INTL"&&t.colour==3;old_codes|=t.text=="HECA  CAI"&&t.colour==3;
            new_name|=t.text=="HONG KONG INTL";
        }
        check(old_name&&old_codes&&!new_name,"pending details keep HECA name codes and coordinates yellow instead of showing draft airport");
        draft.query="NO MATCH";search_airports(draft,sample,0,0);
        check(!confirm_airport_candidate(draft)&&draft.confirmed_airport->icao=="HECA"&&draft.airport_modifying,"invalid draft cannot overwrite confirmed airport or clear pending state");
        check(!control(nd_controls(draft,m),UiAction::display_airport).enabled,"retained old airport details do not enable DISPLAY for invalid draft");
        select_airport_candidate(draft,sample,"VHHH");check(confirm_airport_candidate(draft),"DISPLAY accepts valid candidate");
        auto applied=nd_controls(draft,m);bool new_green=false;
        for(const auto& t:applied.texts)new_green|=t.text=="VHHH  HKG"&&t.colour==2;
        check(draft.confirmed_airport->icao=="VHHH"&&!draft.airport_modifying&&new_green,"confirmed candidate replaces details and returns to green");
        UiState opposite;check(!opposite.airport_modifying&&!opposite.confirmed_airport,"pending airport state is independent per ND");
    }
    s.panel=Panel::airport;s.initial='Z';search_airports(s,index,30,120);
    check(s.results.size()==125&&s.results[s.selection].icao=="Z1000","alphabet locator keeps airports before and after the chosen initial");
    s.query="zsqd";search_airports(s,index,30,120);check(s.results.size()==125&&s.results[s.selection].header_id=="XZ001I","typed ICAO prefix locates canonical source in complete list");
    search_airports(s,index,std::numeric_limits<double>::quiet_NaN(),120);check(s.results.size()==125,"alphabet browser does not depend on aircraft position");
    s.airport_mode=1;s.query.clear();select_airport_candidate(s,index,"VABB");
    check(s.initial=='B'&&s.results.size()==3&&s.results[s.selection].iata=="BOM","selecting BOM retains AAA and CCC in the same IATA list");
    for(char letter:{'A','B','C'}) {locate_airport_initial(s,letter);check(s.results.size()==3&&s.results[s.selection].iata[0]==letter,"A B C move selection without filtering the list");}
    s.query="bom";search_airports(s,index,0,0);
    check(s.results.size()==3&&s.results[s.selection].icao=="VABB","IATA typed prefix resolves actual BOM in full list");
    s.airport_mode=2;s.query="chhat";search_airports(s,index,0,0);
    check(s.results.size()==125&&s.results[s.selection].icao=="VAAA","CITY NAME locates airport name and sorts equal names by ICAO");
    s.query="MUM";search_airports(s,index,0,0);check(s.selection==-1&&s.results.size()==125,"CITY NAME does not search city metadata");
    check(airport_key(missing_codes,2)=="CHHATRAPATI SHIVAJI","airport name is indexed even without city metadata");
    s.query="NO SUCH AIRPORT";search_airports(s,index,0,0);check(s.selection==-1&&!control(nd_controls(s,m),UiAction::display_airport).enabled,"empty group cannot display stale airport");
    s.airport_mode=0;s.query="Z10";search_airports(s,index,0,0);s.airport_dropdown=true;
    scroll_airports(s,100000);f=nd_controls(s,m);
    check(s.airport_first==122,"continuous airport list reaches last three rows beyond 80 airports");
    check(control(f,UiAction::result).box.t<=78,"airport menu drops below field instead of opening above it");
    check(control(f,UiAction::dropdown_thumb).box.b==2,"scroll thumb reaches bottom at end of full list");
    check(f.hit({171,1})->action==UiAction::dropdown_background,"dropdown border shields underlying radio button");
    check(scroll_airport_dropdown(s,{220,39},100)&&s.airport_first==22,"positive wheel scrolls toward earlier airport rows");
    check(scroll_airport_dropdown(s,{220,39},100)&&s.airport_first==0,"wheel clamps at list start");
    check(!scroll_airport_dropdown(s,{350,39},-1),"wheel outside dropdown is not consumed by list");
    const auto selected=s.selection;const auto range=s.range_nm;
    check(scroll_airport_dropdown(s,{282,91},-1)&&s.airport_first==1&&s.selection==selected&&s.range_nm==range,"wheel over selector header scrolls without selecting or zooming");
    s.airport_dropdown=false;s.initial_dropdown=true;s.initial_first=24;
    check(scroll_airport_dropdown(s,{130,40},100)&&s.initial_first==0,"letter dropdown wheel reaches A");
    check(scroll_airport_dropdown(s,{130,40},-100)&&s.initial_first==0,"two available airport initials need no scrolling");
    s.initial_dropdown=false;
    std::vector<IndexEntry> scroll_index;
    for(char letter:{'A','B','C'})for(int i=0;i<5;++i) {
        IndexEntry e=bom;e.icao=e.iata=e.name=std::string(1,letter)+std::to_string(i);scroll_index.push_back(e);
    }
    for(int mode=0;mode<3;++mode) {
        UiState browser;browser.panel=Panel::airport;browser.airport_mode=mode;
        search_airports(browser,scroll_index,0,0);locate_airport_initial(browser,'B');browser.airport_dropdown=true;
        check(browser.results.size()==15&&browser.airport_first==5,"initial B opens at B inside full ICAO IATA or city list");
        check(scroll_airport_dropdown(browser,{220,40},1)&&airport_key(browser.results[browser.airport_first],mode)=="A4","wheel crosses backward from B into A");
        check(scroll_airport_dropdown(browser,{220,40},-6)&&airport_key(browser.results[browser.airport_first],mode)=="C0","wheel crosses forward from B into C");
        locate_airport_initial(browser,'Z');check(browser.results.size()==15&&browser.airport_first==12,"missing initial lands at end without hiding other letters");
        browser.airport_dropdown=false;check(!scroll_airport_dropdown(browser,{220,40},1),"closed dropdown does not intercept wheel");
    }
    s.query="ZSQD";search_airports(s,index,0,0);s.airport_dropdown=false;f=nd_controls(s,m);
    int fms_buttons=0;
    const std::string empty_roles[]={"ORIG","DEST","ALTN"};
    for(const auto& c:f.controls)if(c.action==UiAction::fms_airport){++fms_buttons;check(!c.enabled&&c.label==empty_roles[c.argument],"unavailable FMS airports show disabled ORIG DEST ALTN placeholders");}
    check(fms_buttons==3,"three vertical FMS shortcuts");
    m.fms_origin="ZUCK";m.fms_destination="ZUUU";m.fms_alternate="ZALT";
    const auto shortcuts=nd_controls(s,m);
    const std::string expected[]={"ZUCK","ZUUU","ZALT"};
    for(const auto& c:shortcuts.controls)if(c.action==UiAction::fms_airport)
        check(c.enabled&&c.label==expected[c.argument]&&c.box.b==78-c.argument*36,"right shortcuts are origin destination alternate from top to bottom");
    for(int dropdown=0;dropdown<3;++dropdown) {
        s.initial_dropdown=dropdown==1;s.airport_dropdown=dropdown==2;f=nd_controls(s,m);
        for(const auto& c:f.controls) {
            check(c.box.l>=0&&c.box.b>=0&&c.box.r<=600&&c.box.t<=600,"airport selector controls remain inside ND");
            if(c.box.l==c.box.r||c.box.b==c.box.t||c.action==UiAction::dropdown_background)continue;
            const auto* hit=f.hit({(c.box.l+c.box.r)/2,(c.box.b+c.box.t)/2});
            if(c.action==UiAction::airport_mode&&dropdown!=0) {
                check(hit&&(hit->action==c.action||hit->action==UiAction::result||hit->action==UiAction::initial_select||hit->action==UiAction::dropdown_background),"open dropdown shields covered radio targets");
            } else check(hit&&hit->action==c.action&&hit->argument==c.argument,"each visible alphabet and airport control has a matching hit region");
        }
    }
    for(int mode:{2,3,4}) {
        const auto vp=map_viewport(mode,true,110);
        check(vp.bottom==112&&vp.anchor.y>132&&vp.anchor.y+vp.radius<552,"ARPT SEL compact viewport clears panel and header");
    }
    s.initial_dropdown=s.airport_dropdown=false;s.panel=Panel::map_data;
    for(auto p:{Vec2{-500,-500},Vec2{599,599},Vec2{300,300}}) {
        show_context(s,p,{7,9});check(s.menu_at.x>=8&&s.menu_at.x+210<=592&&s.menu_at.y-192>=122&&s.menu_at.y<=550,"six-row context menu stays above the compact open panel");
        check(s.context_world.x==7&&s.context_world.y==9,"menu clamp does not move geographic mark");
    }
    s.panel=Panel::closed;s.follow=true;
    show_context(s,{-500,-500},{7,9});
    check(s.menu_at.y-192>=55,"closed-panel context menu stays clear of lower map controls");
    const char* menu_labels[]={"ADD CROSS","ADD FLAG","MENU","ERASE CROSSES","ERASE FLAGS","CENTER ON ACFT"};
    const UiAction menu_actions[]={UiAction::add_cross,UiAction::add_flag,UiAction::map_data,UiAction::erase_crosses,UiAction::erase_flags,UiAction::center};
    const auto context=nd_controls(s,m);
    check(context.controls.size()==7,"six context rows plus the existing panel arrow");
    for(int i=0;i<6;++i) {
        Vec2 p={s.menu_at.x+100,s.menu_at.y-i*32-16};
        const auto* hit=context.hit(p);
        check(hit&&hit->label==menu_labels[i]&&hit->action==menu_actions[i],"video-reference context row label, order and hit action");
    }
    check(!control(context,UiAction::erase_crosses).enabled&&!control(context,UiAction::erase_flags).enabled,"empty mark lists disable erase-all rows");
    s.crosses={{7,9}};s.flags={{10,12}};
    const auto marked_context=nd_controls(s,m);
    check(control(marked_context,UiAction::erase_crosses).enabled&&control(marked_context,UiAction::erase_flags).enabled,"existing marks enable erase-all rows");
    s.menu=false;s.panel=Panel::closed;s.range_nm=1;zoom(s,true);check(s.range_nm==.5,"zoom in");
    for(int i=0;i<10;++i)zoom(s,true);
    check(s.range_nm==.2,"zoom stops at minimum");
    for(int i=0;i<10;++i)zoom(s,false);
    check(s.range_nm==5,"zoom stops at maximum");
    {
        UiState dynamic;dynamic.panel=Panel::map_data;dynamic.feature_kind=1;dynamic.feature_initial_dropdown=true;
        UiModel model;std::vector<MapFeature> only_a={{"A1",{}},{"A2",{}}};model.features[1]=&only_a;
        auto letters=[](const UiFrame& frame,UiAction action) {
            std::string result;for(const auto& c:frame.controls)if(c.action==action)result+=c.label;return result;
        };
        check(letters(nd_controls(dynamic,model),UiAction::feature_initial_select)=="A","A-only taxiways expose only A on left");
        only_a.push_back({"c2",{}});only_a.push_back({"101",{}});
        check(map_feature_initials(dynamic,model)=="#AC","feature initials normalize case and put numeric group first");
        dynamic.feature_initial_first=24;
        check(letters(nd_controls(dynamic,model),UiAction::feature_initial_select)=="#AC","shrinking list clamps stale first row");
        only_a.insert(only_a.end(),{{"D1",{}},{"E1",{}},{"F1",{}}});
        scroll_initials(dynamic,model,true,100);
        check(dynamic.feature_initial_first==3&&letters(nd_controls(dynamic,model),UiAction::feature_initial_select)=="DEF","sparse initials scroll and clamp to actual final three letters");
        scroll_initials(dynamic,model,true,-100);
        check(dynamic.feature_initial_first==0&&letters(nd_controls(dynamic,model),UiAction::feature_initial_select)=="#AC","sparse initials scroll back without inventing missing B");
        only_a.clear();
        check(letters(nd_controls(dynamic,model),UiAction::feature_initial_select).empty(),"empty features expose no stale initials");
        dynamic.panel=Panel::airport;dynamic.initial_dropdown=true;
        IndexEntry entry;entry.icao="ZAAA";entry.iata="AAA";entry.name="alpha";
        IndexEntry second=entry;second.icao="ZBBB";second.iata="CCC";second.name="Beta";
        for(int mode=0;mode<3;++mode) {
            dynamic.airport_mode=mode;search_airports(dynamic,{entry,second},0,0);
            const std::string expected[]={"Z","AC","AB"};
            check(letters(nd_controls(dynamic,model),UiAction::initial_select)==expected[mode],"airport initials follow current ICAO IATA or city list");
            IndexEntry numeric=entry;numeric.icao=numeric.iata=numeric.name="00CA";
            search_airports(dynamic,{entry,numeric,second},0,0);
            check(dynamic.airport_initials=="#"+expected[mode]&&airport_key(dynamic.results.front(),mode)=="00CA","numeric group comes first in both airport selectors for every mode");
            locate_airport_initial(dynamic,'#');
            check(dynamic.selection==0,"hash jumps to first numeric airport");
            locate_airport_initial(dynamic,expected[mode][0]);
            check(dynamic.selection==1,"letter jump follows numeric airport group");
        }
        search_airports(dynamic,{},0,0);
        check(dynamic.airport_initials.empty()&&letters(nd_controls(dynamic,model),UiAction::initial_select).empty(),"empty airport index clears initials safely");
        UiState accelerated;accelerated.panel=Panel::airport;accelerated.airport_dropdown=true;accelerated.results.resize(1000);
        for(int i=0;i<=15;++i)scroll_nd_dropdown(accelerated,model,{220,40},-1,10+i*0.1);
        check(accelerated.airport_first>40,"sustained wheel events accelerate actual airport scrolling");
        auto before=accelerated.airport_first;
        scroll_nd_dropdown(accelerated,model,{220,40},1,11.6);
        check(accelerated.airport_first==before-1,"reversing wheel restores one-row precision");
        scroll_nd_dropdown(accelerated,model,{220,40},-1,12.2);
        check(accelerated.airport_first==before,"pause resets acceleration");
        UiState::WheelAcceleration wheel;
        check(wheel.apply(-1,1,0)==-1,"first wheel tick moves one row");
        int last=0;for(int i=1;i<=60;++i)last=wheel.apply(-1,1,i*.1);
        check(last==-16,"continuous wheel acceleration caps at sixteen rows per tick");
        check(wheel.apply(-1,2,6.1)==-1,"changing dropdown resets acceleration");
        check(wheel.apply(0,2,6.2)==0,"zero clicks never move list");
        UiState independent;check(independent.wheel.apply(-1,1,6.2)==-1,"other ND has independent wheel acceleration");
        scroll_nd_dropdown(accelerated,model,{500,400},-1,12.3);
        check(accelerated.wheel.last<0,"leaving dropdown clears acceleration before map zoom");
    }
    UiState other;check(other.range_nm==1,"opposite ND zoom is independent");
    Gesture g;g.begin({100,100},{10,10},true);check(!g.move({14,14})&&!g.moved,"small pointer jitter remains a click");
    check(g.move({20,20})&&g.moved,"drag uses desktop distance threshold");g.clear();check(!g.pressed,"release clears capture");
    g.begin({100,100},{10,10},false);check(!g.move({40,40})&&g.moved,"dragging a panel cannot pan map or become a click");
    MapLabels labels;
    auto first=labels.place({100,200},50,16,"A");check(bool(first),"place visible taxiway caption");
    check(!labels.place({130,200},50,16,"A"),"avoid repeated nearby labels along the same taxiway");
    auto other_label=labels.place({100,200},50,16,"B");
    check(other_label&&(other_label->b>=first->t||other_label->t<=first->b||other_label->l>=first->r||other_label->r<=first->l),"colliding captions move without overlapping");
    check(!labels.place({-200,200},50,16)&&!labels.place({200,590},50,16),"captions do not cover OANS header or leave map");
    check(bool(labels.place({300,200},50,16,"A")),"repeat taxiway name when sufficiently far from previous caption");
}
// Export the actual shared layout, not an independently drawn UI mockup.
void preview(const std::string& path) {
    UiState s;UiModel m;m.code="ZSQD";m.name="Qingdao Jiaodong Intl";m.runway="17";m.profile=true;m.index_ready=true;m.runways=4;
    std::ifstream cycle_info("G:/SteamLibrary/steamapps/common/X-Plane 12/Custom Data/cycle_info.txt");
    m.database=read_airport_database_status(cycle_info);
    std::cout<<"Preview database: "<<m.database.cycle<<" "<<m.database.validity<<'\n';
    std::ofstream o(path);o<<"<svg xmlns='http://www.w3.org/2000/svg' width='1800' height='600' viewBox='0 0 1800 600'>";
    for(int page=1;page<=3;++page) {
        s.panel=static_cast<Panel>(page);s.query="ZSQD";s.editing=page==2;
        IndexEntry a;a.icao="ZSQD";a.icao_explicit=true;a.name=m.name;s.results={a};s.selection=0;
        auto f=nd_controls(s,m);o<<"<g transform='translate("<<(page-1)*600<<" 0)'><rect width='600' height='600' fill='#050b10'/>";
        for(const auto& b:f.backgrounds)o<<"<rect x='"<<b.l<<"' y='"<<600-b.t<<"' width='"<<b.r-b.l<<"' height='"<<b.t-b.b<<"' fill='"<<(f.status_style?"#66667a":"#101c24")<<"'/>";
        for(const auto& b:f.rules)o<<"<rect x='"<<b.l<<"' y='"<<600-b.t<<"' width='"<<b.r-b.l<<"' height='"<<b.t-b.b<<"' fill='#ffffff'/>";
        for(const auto& c:f.controls) {const auto& b=c.box;std::string label=c.label;for(size_t p=0;(p=label.find('&',p))!=std::string::npos;p+=5)label.replace(p,1,"&amp;");for(size_t p=0;(p=label.find('<',p))!=std::string::npos;p+=4)label.replace(p,1,"&lt;");
            o<<"<rect x='"<<b.l<<"' y='"<<600-b.t<<"' width='"<<b.r-b.l<<"' height='"<<b.t-b.b<<"' fill='"<<(f.status_style?(c.selected||c.action==UiAction::close?"#66667a":"#1a1a1a"):c.selected?"#0b3740":"#101c24")<<"' stroke='"<<(f.status_style?(c.selected?"#66667a":"#ffffff"):"#35505a")<<"'/><text x='"<<b.l+7<<"' y='"<<600-(b.b+b.t)/2+5<<"' fill='"<<(c.enabled?(f.status_style&&!c.selected?"#ffffff":"#00e5ff"):"#687878")<<"' font-size='14' font-family='monospace'>"<<label<<"</text>";
        }
        for(const auto& t:f.texts)o<<"<text x='"<<t.at.x<<"' y='"<<600-t.at.y<<"' fill='"<<(t.colour==1?"#ffa020":t.colour==2?"#00ff00":"#ffffff")<<"' font-size='"<<t.size<<"' font-family='monospace'>"<<t.text<<"</text>";
        o<<"</g>";
    }o<<"</svg>";
}
int main(int argc,char** argv) {try {tests();if(argc>1)preview(argv[1]);std::cout<<"PASS "<<checks<<" ND UI checks\n";return 0;}catch(const std::exception& e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
