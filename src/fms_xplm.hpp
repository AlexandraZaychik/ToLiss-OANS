#pragma once
#include "fms.hpp"
#include <XPLMDataAccess.h>
#include <XPLMUtilities.h>
#include <vector>

namespace oans {
class McduFmsPort {
public:
    FmsSampler sampler;
    bool bind();
    void tick(double now,bool usable);
    void clear(bool restore);
    bool ready() const {return !text_.empty();}
private:
    std::map<std::string,XPLMDataRef> text_;
    std::map<std::string,XPLMCommandRef> pages_;
    std::vector<XPLMCommandRef> observed_;
    bool issuing_=false;
    double next_read_=0;
    McduScreen read() const;
    void send(const std::string& suffix);
    static int activity(XPLMCommandRef,XPLMCommandPhase,void*);
};
}
