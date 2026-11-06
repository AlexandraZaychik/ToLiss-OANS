// Load the actual plugin against the mock SDK; never attaches to X-Plane.
#include <windows.h>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
#include "menu_layout.hpp"

template<class T>T symbol(HMODULE module,const char* name) {
    auto address=GetProcAddress(module,name);
    if(!address)throw std::runtime_error(std::string("Missing symbol: ")+name);
    return reinterpret_cast<T>(address);
}
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
int main(int argc,char** argv) {
    if(argc!=5&&argc!=6) {std::cerr<<"MOCK_DLL PLUGIN SIM MODEL [--reverse]\n";return 2;}
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);
    HMODULE mock=nullptr,plugin=nullptr;void(*stop)()=nullptr;
    try {
        mock=LoadLibraryExW(std::filesystem::u8path(argv[1]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        require(mock!=nullptr,"load mock SDK");
        symbol<void(*)(const char*,const char*,const char*)>(mock,"MockConfigure")(argv[2],argv[3],argv[4]);
        plugin=LoadLibraryExW(std::filesystem::u8path(argv[2]).c_str(),nullptr,LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR|LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        if(!plugin)throw std::runtime_error("load real plugin binary: Windows error "+std::to_string(GetLastError()));
        stop=symbol<void(*)()>(plugin,"XPluginStop");
        char name[256],signature[256],description[256];
        require(symbol<int(*)(char*,char*,char*)>(plugin,"XPluginStart")(name,signature,description)==1,"start plugin");
        require(symbol<int(*)()>(plugin,"XPluginEnable")()==1,"enable plugin");
        const auto count=symbol<int(*)()>(mock,"MockMenuItems");
        const auto label=symbol<const char*(*)(int)>(mock,"MockMenuLabel");
        const auto click=symbol<void(*)(int)>(mock,"MockMenuClick");
        const auto checked=symbol<int(*)(int)>(mock,"MockMenuChecked");
        const auto focus=symbol<int(*)()>(mock,"MockFocus");
        const auto key=symbol<void(*)(int)>(mock,"MockKey");
        std::vector<const oans::MenuEntry*> expected;
        std::function<void(const char*)> visit=[&](const char* parent) {
            std::vector<const oans::MenuEntry*> children;
            for(const auto& item:oans::menu_layout)if(std::string(item.parent)==parent)children.push_back(&item);
            if(argc==6)std::reverse(children.begin(),children.end());
            for(const auto item:children){expected.push_back(item);visit(item->id);}
        };
        visit("root");
        require(count()==static_cast<int>(expected.size()),"menu entry count");
        for(int i=0;i<count();++i)require(std::string(label(i))==expected[i]->label,"menu order follows definition");
        auto index=[&](int action) {
            for(int i=0;i<count();++i)if(expected[i]->action==action)return i;
            throw std::runtime_error("action not found");
        };
        auto is_checked=[&](int action){return checked(index(action))!=0;};
        auto invoke=[&](int action){click(index(action));};
        require(is_checked(20)&&!is_checked(21),"Simple default checkmark");
        invoke(21);require(!is_checked(20)&&is_checked(21),"Complex checkmark after reorder");
        invoke(20);require(is_checked(20)&&!is_checked(21),"Simple checkmark after reorder");
        for(int action=24;action<=27;++action) {
            invoke(action);
            for(int other=24;other<=27;++other)require(is_checked(other)==(other==action),"cursor checkmark exclusivity after reorder");
        }
        require(is_checked(28)&&!is_checked(31),"MCDU full open defaults on");
        invoke(31);require(!is_checked(28)&&is_checked(31),"MCDU full close is exclusive");
        invoke(31);require(!is_checked(28)&&is_checked(31),"MCDU full close is idempotent");
        symbol<void(*)(const char*)>(mock,"MockCommand")("toliss_oans/reload");
        require(!is_checked(28)&&is_checked(31),"MCDU full close persists after reload");
        invoke(28);invoke(28);require(is_checked(28)&&!is_checked(31),"MCDU full open is exclusive and idempotent");
        require(!is_checked(32),"Close oans when TO defaults off");
        invoke(32);
        symbol<void(*)(const char*)>(mock,"MockCommand")("toliss_oans/reload");
        require(is_checked(32),"Close oans when TO enabled state survives reload after reorder");
        invoke(32);
        for(int action:{22,29,32}) {
            const bool before=is_checked(action);invoke(action);
            require(is_checked(action)!=before,"toggle checkmark after reorder");invoke(action);
            require(is_checked(action)==before,"toggle restored");
        }
        for(auto action:{19,23,30}) {
            invoke(action);require(focus()==(action==19?2:action==23?3:4),"window action retains callback");key(27);
        }
        symbol<void(*)(const char*)>(mock,"MockCommand")("toliss_oans/reload");
        require(is_checked(20)&&is_checked(27)&&is_checked(28)&&!is_checked(31),"reload retains checkmarks by action ID");
        stop();stop=nullptr;
        require(symbol<int(*)()>(mock,"MockResources")()==0,"cleanup releases SDK registrations");
        FreeLibrary(plugin);plugin=nullptr;FreeLibrary(mock);mock=nullptr;
        std::cout<<"PASS real plugin menu order, callbacks, checkmarks, reload and cleanup (MOCK SDK ONLY)\n";
        return 0;
    } catch(const std::exception& error) {
        if(stop)stop();
        if(plugin)FreeLibrary(plugin);
        if(mock)FreeLibrary(mock);
        std::cerr<<error.what()<<'\n';return 1;
    }
}
