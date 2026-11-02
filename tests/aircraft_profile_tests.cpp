#include "aircraft_profile.hpp"
#include "aircraft/lr/identity.hpp"
#include "aircraft/lr/display.hpp"
#include <iostream>
#include <map>
#include <stdexcept>
void check(bool ok,const char* reason) {if(!ok)throw std::runtime_error(reason);}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Supply source directory");
        const auto source=std::filesystem::path(argv[1])/"config/toliss";
        std::string error;
        check(profile::load(source,"A20N",error),error.c_str());
        const auto original=profile::current;
        check(original.nd_brightness[0]==1&&original.nd_brightness[1]==3,"Brightness mapping");
        check(std::abs(original.nd_uv[0][0][0]-.18650067)<1e-10,"UV mapping");
        check(profile::load(source,"a20n",error),"Lowercase ICAO");
        const auto temp=std::filesystem::temp_directory_path()/"oans-profile-regression";
        std::filesystem::create_directories(temp);
        const auto path=temp/"a20n.json";
        std::ifstream in(source/"a20n.json");
        const std::string good((std::istreambuf_iterator<char>(in)),std::istreambuf_iterator<char>());
        auto write=[&](const std::string& text){std::ofstream out(path);out<<text;};
        auto invalid=[&](const std::string& text){write(text);check(!profile::load(temp,"A20N",error),"Invalid config accepted");check(!error.empty(),"Error missing");check(profile::current.nd_uv[0][0][0]==0,"Stale profile after failure");};
        invalid("{}");invalid(good+"garbage");invalid("{\"nd_brightness\":[1,3],}");
        auto replace=[&](const std::string& before,const std::string& after){auto text=good;auto at=text.find(before);check(at!=std::string::npos,"Fixture token missing");text.replace(at,before.size(),after);return text;};
        invalid(replace("0.18650067","1.1"));
        invalid(replace("0.18650067","NaN"));
        invalid(replace("0.18650067","01"));
        invalid(replace("0.18650067","+0.1"));
        invalid(replace("0.18650067","0.99"));
        invalid(replace("\"nd_uv\"","\"unknown\""));
        invalid(replace("\"pfd_brightness\"","\"nd_brightness\""));
        invalid(replace("    3\n","    3, 4\n"));
        check(!profile::load(temp,"A319",error),"Missing model accepted");
        check(!profile::load(source,"../x",error),"Invalid ICAO accepted");
        write(replace("0.18650067","0.18"));
        check(profile::load(temp,"A20N",error),"Reload failed");
        check(profile::current.nd_uv[0][0][0]==.18,"Edited profile not reloaded");
        const auto lr_directory=std::filesystem::path(argv[1])/"config/lr";
        check(profile::load(lr_directory,"A333",error),error.c_str());
        check(profile::current.nd_brightness[0]==1&&profile::current.nd_brightness[1]==4,"LR ND brightness channels");
        check(profile::current.pfd_brightness[0]==0&&profile::current.pfd_brightness[1]==5,"LR PFD brightness channels");
        check(profile::current.nd_uv[0][0][1]>.5&&profile::current.nd_uv[0][1][3]<.5,"LR captain/FO panel halves");
        check(profile::current.nd_corners[0][0][0]<0&&profile::current.nd_corners[1][0][0]>0,"LR captain/FO geometry sides");
        for(int side=0;side<2;++side)for(int axis=0;axis<4;++axis)
            check(profile::current.nd_uv[0][side][axis]==profile::current.nd_uv[1][side][axis],"LR has one texture resolution layout");
        std::map<std::string,std::vector<double>> values{{"bright",std::vector<double>(32,1)}};
        for(int side=0;side<2;++side) {
            for(const char* key:{"att","hdg","lrn"})values[oans::lr::display::key(key,side)]={1};
            values[oans::lr::display::key("mode",side)]={3};values[oans::lr::display::key("range",side)]={10};
            values[oans::lr::display::key("nd_state",side)]={3};values[oans::lr::display::key("map",side)]={2};
            values[oans::lr::display::key("swap",side)]={0};values[oans::lr::display::key("ecam",side)]={0};
        }
        auto numeric=[&](const std::string& key,int index){const auto found=values.find(key);return found!=values.end()&&index>=0&&index<int(found->second.size());};
        oans::aircraft::DataAccess data{
            [&](const std::string& key,int index,double fallback){return numeric(key,index)?values.at(key)[index]:fallback;},numeric};
        auto allowed=[&](int side){return oans::lr::display::overlay_blocked(data,side,false)[0]=='\0';};
        check(oans::lr::display::ready(data)&&allowed(0)&&allowed(1),"Aligned LR NAV displays support OANS");
        for(int mode=0;mode<=5;++mode) {values["mode0"][0]=mode;check(allowed(0)==(mode>=2&&mode<=4),"LR physical selector mode mapping");}
        values["mode0"][0]=3;
        values["range0"][0]=20;check(!allowed(0)&&allowed(1),"LR 10 NM gate is per side");values["range0"][0]=10;
        for(int state=0;state<3;++state) {values["nd_state0"][0]=state;check(!allowed(0),"LR off/warmup/self-test must retain original display");}
        values["nd_state0"][0]=3;
        for(const char* gate:{"swap0","ecam0"}) {values[gate][0]=1;check(!allowed(0)&&allowed(1),"LR display switching blocks its own side");values[gate][0]=0;}
        values["bright"][1]=0;check(!allowed(0)&&allowed(1),"LR captain brightness uses channel 1");values["bright"][1]=1;
        values["bright"][4]=0;check(allowed(0)&&!allowed(1),"LR FO brightness uses channel 4");values["bright"][4]=1;
        for(const char* gate:{"att0","hdg0","lrn0"}) {
            values[gate][0]=0;check(!allowed(0)&&!oans::lr::display::position_ready(data,0)&&oans::lr::display::position_ready(data,-1),"LR requires attitude, heading and long-range navigation validity");values[gate][0]=1;
        }
        values["map0"][0]=1;check(!allowed(0),"Heading-only LR map must not be overlaid");values["map0"][0]=2;
        values.erase("swap0");check(!oans::lr::display::ready(data),"Missing LR switching interface leaves adapter unready");
        const auto model=temp/"renamed.acf";
        auto identity=[&](const std::string& text){std::ofstream out(model);out<<text;out.close();return oans::lr::matches_model(model);};
        check(identity("P acf/_studio Laminar Research\r\nP acf/_ICAO C172\r\n"),"Studio identifies LR independently of ICAO and folder name");
        check(identity("P acf/_studio Laminar Research \t\r\n"),"Studio trims trailing whitespace");
        check(!identity("P acf/_manufacturer Laminar Research\nP acf/_ICAO A333\n"),"Manufacturer/ICAO alone cannot identify LR");
        check(!identity("P acf/_studio Other Laminar Research\n"),"Studio must equal Laminar Research");
        check(!identity("PANEL_3D_BEGIN\nP acf/_studio Laminar Research\n"),"Only ACF metadata identifies studio");
        check(!oans::lr::matches_model(temp/"missing.acf"),"Missing ACF cannot identify LR");
        std::filesystem::remove(model);std::filesystem::remove(path);std::filesystem::remove(temp);
        std::cout<<"PASS aircraft JSON profiles, LR studio identity, validation, failure reset and reload\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
