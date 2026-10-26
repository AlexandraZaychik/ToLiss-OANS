#include "detail_log.hpp"
#include <fstream>
#include <iostream>
#include <thread>
#include <vector>
#include <stdexcept>

void check(bool ok,const char* reason){if(!ok)throw std::runtime_error(reason);}
std::string read(const std::filesystem::path& path){std::ifstream f(path);return {std::istreambuf_iterator<char>(f),{}};}
void nested(){OANS_TRACE_SCOPE();OANS_TRACE("nested marker");}
int main(int argc,char** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    try {
        check(argc>=2,"expected fresh test directory");
        const auto directory=std::filesystem::u8path(argv[1]);std::filesystem::create_directories(directory);
        check(oans::detail::initialize(directory,"logger-test"),"open log");
        if(argc==3&&std::string(argv[2])=="--fault") {
            OANS_TRACE_SCOPE();OANS_TRACE("before synthetic unhandled access violation");
            ULONG_PTR parameters[2]={0,0x1234};RaiseException(EXCEPTION_ACCESS_VIOLATION,0,2,parameters);
            return 2;
        }
        OANS_CONFIG("config marker");
        int side_effect=0;OANS_TRACE(std::to_string(++side_effect));
        check(side_effect==(OANS_DEBUG?1:0),"release must not evaluate trace expressions");
        std::vector<std::thread> workers;
        for(int t=0;t<4;++t)workers.emplace_back([t]{for(int i=0;i<100;++i)OANS_CONFIG("worker="+std::to_string(t)+" record="+std::to_string(i));nested();});
        for(auto& worker:workers)worker.join();
        const auto first=read(directory/"detail.log");
        check(first.find("config marker")!=std::string::npos,"records visible before close");
        check((first.find("ENTER]")!=std::string::npos)==bool(OANS_DEBUG),"trace flavor separation");
        for(int t=0;t<4;++t)for(int i=0;i<100;++i)
            check(first.find("worker="+std::to_string(t)+" record="+std::to_string(i)+"\n")!=std::string::npos,"concurrent records must not interleave");
        oans::detail::shutdown();
        check(oans::detail::initialize(directory,"restart"),"reopen log");
        check(read(directory/"detail.log").find("config marker")!=std::string::npos,"restart preserves earlier session");
        oans::detail::shutdown();
        std::filesystem::resize_file(directory/"detail.log",32ull*1024*1024);
        check(oans::detail::initialize(directory,"rotation"),"reopen after size limit");
        check(std::filesystem::exists(directory/"detail.log.1"),"rotation keeps previous file");
        check(read(directory/"detail.log").find("version=rotation")!=std::string::npos,"new log has session header");
        oans::detail::shutdown();
        std::cout<<"PASS "<<oans::detail::flavor<<" logger: flush, threads, append, rotation, flavor separation\n";return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';oans::detail::shutdown();return 1;}
}
