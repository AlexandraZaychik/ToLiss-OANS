// Execute the production C++ state machine against real screens supplied by
// sample_mcdu2_fms.py. This executable is a development tool, never shipped.
#include "fms.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
int main() {
    oans::FmsSampler sampler;std::string line;
    try {
        while(std::getline(std::cin,line)) {
            std::istringstream in(line);double now;int count;
            if(!(in>>now>>count)||count<0||count>200)throw std::runtime_error("bad screen header");
            oans::McduScreen s;
            for(int i=0;i<count;++i) {
                if(!std::getline(std::cin,line))throw std::runtime_error("truncated screen");
                if(!line.empty()&&line.back()=='\r')line.pop_back();
                auto tab=line.find('\t');if(tab==std::string::npos)throw std::runtime_error("bad text row");
                s.fields[line.substr(0,tab)]=line.substr(tab+1);
            }
            auto command=sampler.tick(now,s);const auto& p=sampler.plan();
            std::cout<<command<<'\t'<<sampler.status()<<'\t'<<p.observed<<'\t'<<p.origin.airport<<'\t'<<p.origin.runway
                <<'\t'<<p.destination.airport<<'\t'<<p.destination.runway<<'\t'<<sampler.busy()<<'\t'<<oans::mcdu_page_name(sampler.saved_page())<<'\t'<<p.alternate<<std::endl;
        }
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
