// Exercises the plugin logger's exception observer in a disposable child.
#include "detail_log.hpp"
#include <iostream>
int main(int argc,char** argv){
#ifdef _WIN32
 SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
 if(argc!=3||std::string(argv[2])!="--fault")return 2;
 std::filesystem::create_directories(std::filesystem::u8path(argv[1]));
 if(!oans::detail::initialize(std::filesystem::u8path(argv[1]),"logger-test"))return 3;
 OANS_TRACE_SCOPE();OANS_TRACE("before synthetic unhandled access violation");
 ULONG_PTR parameters[2]={0,0x1234};RaiseException(EXCEPTION_ACCESS_VIOLATION,0,2,parameters);
#endif
 return 2;
}
