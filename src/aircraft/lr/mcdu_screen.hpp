#pragma once
#include "mcdu.hpp"
#include <array>
#include <regex>

namespace oans::lr {
// Translate the default Airbus CDU's 24-column rows into the sampler's fields.
inline McduScreen mcdu_screen(const std::array<std::string,16>& rows) {
    McduScreen s;
    auto title=rows[0];
    // DATA INDEX has a page counter and arrow glyphs on the same row.
    if(title.find("DATA INDEX")!=std::string::npos)title="DATA INDEX";
    for(const std::string glyph:{"↑","↓","←","→"}) {
        for(auto pos=title.find(glyph);pos!=std::string::npos;pos=title.find(glyph))title.erase(pos,glyph.size());
    }
    s.fields["titlew"]=title;
    for(int i=1;i<=6;++i) {
        s.fields["label"+std::to_string(i)+"w"]=rows[2*i-1];
        s.fields["cont"+std::to_string(i)+"w"]=rows[2*i];
    }
    // LR omits FROM in the header. Only the airport+runway endpoint shape
    // on the reset-to-top F-PLN page qualifies; arbitrary four-letter fixes do not.
    static const std::regex origin(R"(^ *[A-Z]{4}(0[1-9]|[12][0-9]|3[0-6])[LCR]?( +.*)?$)");
    if(rows[11].find("DEST")==0&&std::regex_match(rows[2],origin))s.fields["stitlew"]="FROM";
    auto scratch=rows[13];
    for(const std::string glyph:{"↑","↓","←","→"}) {
        for(auto pos=scratch.find(glyph);pos!=std::string::npos;pos=scratch.find(glyph))scratch.erase(pos,glyph.size());
    }
    s.fields["spw"]=scratch;
    // Additional message lines must also prevent background key presses.
    s.fields["spw"]+=rows[14]+rows[15];
    return s;
}
}
