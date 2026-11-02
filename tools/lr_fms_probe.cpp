// Execute the production C++ state machine against real screens supplied by
// sample_mcdu2_fms.py. This executable is a development tool, never shipped.
#include "aircraft/lr/mcdu.hpp"
#include "aircraft/lr/mcdu_screen.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
int main(int argc,char** argv) {
    const bool lr=argc==2&&std::string(argv[1])=="--lr";
    oans::lr::FmsSampler sampler;std::string line;
    try {
        while(std::getline(std::cin,line)) {
            std::istringstream in(line);double now;int count;
            if(!(in>>now>>count)||count<0||count>200)throw std::runtime_error("bad screen header");
            oans::lr::McduScreen s;
            for(int i=0;i<count;++i) {
                if(!std::getline(std::cin,line))throw std::runtime_error("truncated screen");
                if(!line.empty()&&line.back()=='\r')line.pop_back();
                auto tab=line.find('\t');if(tab==std::string::npos)throw std::runtime_error("bad text row");
                s.fields[line.substr(0,tab)]=line.substr(tab+1);
            }
            if(lr) {
                std::array<std::string,16> rows{};
                for(int i=0;i<16;++i)rows[i]=s.fields[std::to_string(i)];
                s=oans::lr::mcdu_screen(rows);
            }
            auto command=sampler.tick(now,s);const auto& p=sampler.plan();
            std::cout<<command<<'\t'<<sampler.status()<<'\t'<<p.observed<<'\t'<<p.origin.airport<<'\t'<<p.origin.runway
                <<'\t'<<p.destination.airport<<'\t'<<p.destination.runway<<'\t'<<sampler.busy()<<'\t'<<oans::lr::mcdu_page_name(sampler.saved_page())<<'\t'<<p.alternate<<std::endl;
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
