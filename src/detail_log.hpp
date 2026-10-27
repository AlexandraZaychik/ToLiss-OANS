#pragma once
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <string>
#include <exception>
#ifndef OANS_DEBUG
#define OANS_DEBUG 0
#endif
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <pthread.h>
#endif

// No simulator APIs here: worker threads and exception reporting may use this.
namespace oans::detail {
inline constexpr bool debug=OANS_DEBUG!=0;
inline constexpr const char* flavor=debug?"debug":"release";
inline std::mutex mutex;
inline std::FILE* file=nullptr;
inline std::filesystem::path path;
inline unsigned long long sequence=0,bytes=0;
inline thread_local unsigned depth=0;
inline auto started=std::chrono::steady_clock::now();
#if defined(_WIN32) && OANS_DEBUG
inline HANDLE emergency=INVALID_HANDLE_VALUE;
inline PVOID handler=nullptr;
inline thread_local bool reporting=false;
inline void emergency_line(const char* line) noexcept {
    if(emergency==INVALID_HANDLE_VALUE)return;
    DWORD written=0;WriteFile(emergency,line,static_cast<DWORD>(std::char_traits<char>::length(line)),&written,nullptr);
}
inline void address_line(const char* label,void* address) noexcept {
    HMODULE module=nullptr;char module_path[2048]="unknown",line[2400];
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                      reinterpret_cast<LPCSTR>(address),&module);
    if(module)GetModuleFileNameA(module,module_path,sizeof(module_path));
    std::snprintf(line,sizeof(line),"%s address=%p module=%s offset=0x%llx\n",label,address,module_path,
        static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(address)-reinterpret_cast<std::uintptr_t>(module)));
    emergency_line(line);
}
inline LONG CALLBACK exception_observer(EXCEPTION_POINTERS* info) noexcept {
    const DWORD code=info->ExceptionRecord->ExceptionCode;
    if(reporting||!(code==EXCEPTION_ACCESS_VIOLATION||code==EXCEPTION_IN_PAGE_ERROR||
       code==EXCEPTION_ILLEGAL_INSTRUCTION||code==EXCEPTION_INT_DIVIDE_BY_ZERO||
       code==EXCEPTION_STACK_OVERFLOW||code==EXCEPTION_PRIV_INSTRUCTION))return EXCEPTION_CONTINUE_SEARCH;
    reporting=true;char line[400];
    std::snprintf(line,sizeof(line),"EXCEPTION first-chance code=0x%08lx thread=%lu scope_depth=%u (may be handled by host; observer never suppresses it)\n",
                  code,GetCurrentThreadId(),depth);emergency_line(line);
    address_line("fault",info->ExceptionRecord->ExceptionAddress);
    if(info->ExceptionRecord->NumberParameters>=2) {
        std::snprintf(line,sizeof(line),"exception parameters operation=%llu target=0x%llx (AV: 0=read 1=write 8=execute)\n",
            static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[0]),
            static_cast<unsigned long long>(info->ExceptionRecord->ExceptionInformation[1]));emergency_line(line);
    }
#if defined(_WIN64)
    const auto* c=info->ContextRecord;
    std::snprintf(line,sizeof(line),"registers RIP=0x%llx RSP=0x%llx RBP=0x%llx RCX=0x%llx RDX=0x%llx R8=0x%llx R9=0x%llx\n",
        c->Rip,c->Rsp,c->Rbp,c->Rcx,c->Rdx,c->R8,c->R9);emergency_line(line);
#endif
    // Observer stack is supplemental; the exception address above is authoritative.
    if(code!=EXCEPTION_STACK_OVERFLOW) {
        void* stack[40]{};const auto count=CaptureStackBackTrace(0,40,stack,nullptr);
        for(USHORT i=0;i<count;++i)address_line("observer-stack",stack[i]);
    }
    if(emergency!=INVALID_HANDLE_VALUE)FlushFileBuffers(emergency);
    reporting=false;return EXCEPTION_CONTINUE_SEARCH;
}
#endif
inline void open_file() noexcept {
#ifdef _WIN32
    file=_wfopen(path.c_str(),L"ab");
#else
    file=std::fopen(path.c_str(),"ab");
#endif
    if(file)std::setvbuf(file,nullptr,_IONBF,0); // every record reaches the OS before returning
#if defined(_WIN32) && OANS_DEBUG
    emergency=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,
                          nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
#endif
}
inline void close_file() noexcept {
#if defined(_WIN32) && OANS_DEBUG
    if(emergency!=INVALID_HANDLE_VALUE){CloseHandle(emergency);emergency=INVALID_HANDLE_VALUE;}
#endif
    if(file){std::fclose(file);file=nullptr;}
}
inline void rotate() {
    close_file();std::error_code ec;
    for(int i=3;i>=1;--i) {
        auto destination=path;destination+="."+std::to_string(i);
        auto source=path;if(i>1)source+="."+std::to_string(i-1);
        std::filesystem::remove(destination,ec);ec.clear();
        std::filesystem::rename(source,destination,ec);
    }
    bytes=0;open_file();
}
inline void write(const char* category,const std::string& message) noexcept {
    try {
        std::lock_guard<std::mutex> lock(mutex);if(!file)return;
        if(bytes>=32ull*1024*1024)rotate();
        if(!file)return;
        auto clean=message;for(char& c:clean)if(c=='\r'||c=='\n')c=' ';
        const auto ms=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now()-started).count();
        unsigned long thread=0;
#ifdef _WIN32
        thread=GetCurrentThreadId();
#elif defined(__APPLE__)
        std::uint64_t identifier=0;pthread_threadid_np(nullptr,&identifier);thread=static_cast<unsigned long>(identifier);
#endif
        char prefix[192];
        const int n=std::snprintf(prefix,sizeof(prefix),"[%llu +%lldms thread=%lu depth=%u %s] ",++sequence,
                                  static_cast<long long>(ms),thread,depth,category);
        if(n<=0||static_cast<std::size_t>(n)>=sizeof(prefix))return;
        const auto record=std::string(prefix)+clean+'\n';
        // Unbuffered fprintf can issue many tiny CRT writes per record. Assemble
        // first, then issue one fwrite so high-frequency tracing stays usable.
        bytes+=std::fwrite(record.data(),1,record.size(),file);
    }catch(...) {} // diagnostics must never turn a recoverable error into a crash
}
inline bool initialize(const std::filesystem::path& directory,const std::string& version) noexcept {
    try {
        {std::lock_guard<std::mutex> lock(mutex);close_file();path=directory/"detail.log";
         std::error_code ec;bytes=std::filesystem::file_size(path,ec);if(ec)bytes=0;
         if(bytes>=32ull*1024*1024)rotate();else open_file();}
        write("CONFIG","SESSION version="+version+" build="+flavor+" compiled="+__DATE__+" "+__TIME__);
#if defined(_WIN32) && OANS_DEBUG
        if(!handler)handler=AddVectoredExceptionHandler(1,exception_observer);
        write("RUNTIME",std::string("exception observer installed=")+(handler?"yes":"no"));
#endif
        return file!=nullptr;
    }catch(...) {return false;}
}
inline void shutdown() noexcept {
#if defined(_WIN32) && OANS_DEBUG
    if(handler){RemoveVectoredExceptionHandler(handler);handler=nullptr;}
#endif
    std::lock_guard<std::mutex> lock(mutex);close_file();
}
struct Scope {
    const char* name;int exceptions;std::chrono::steady_clock::time_point begin;
    explicit Scope(const char* value) noexcept:name(value),exceptions(std::uncaught_exceptions()),begin(std::chrono::steady_clock::now()) {
        try {write("ENTER",name);}catch(...) {}
        ++depth;
    }
    ~Scope() noexcept {
        --depth;
        try {write(std::uncaught_exceptions()>exceptions?"UNWIND":"EXIT",std::string(name)+" us="+
            std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-begin).count()));}catch(...) {}
    }
};
}
#define OANS_CONFIG(message) do {try {::oans::detail::write("CONFIG",(message));}catch(...) {}} while(false)
#if OANS_DEBUG
#define OANS_TRACE_SCOPE() ::oans::detail::Scope oans_detail_scope(__func__)
#define OANS_TRACE(message) do {try {::oans::detail::write("TRACE",(message));}catch(...) {}} while(false)
#else
#define OANS_TRACE_SCOPE() ((void)0)
#define OANS_TRACE(message) ((void)0)
#endif
