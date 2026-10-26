#include "aircraft_profile.hpp"
#include <iostream>
#include <stdexcept>
void check(bool ok,const char* reason) {if(!ok)throw std::runtime_error(reason);}
int main(int argc,char** argv) {
    try {
        check(argc==2,"Supply source directory");
        const auto source=std::filesystem::path(argv[1])/"config";
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
        std::filesystem::remove(path);std::filesystem::remove(temp);
        std::cout<<"PASS aircraft JSON profile loading, validation, failure reset and reload\n";
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
