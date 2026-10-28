#pragma once
#include <curl/curl.h>
#include <memory>
#include <stdexcept>
#include <string>

namespace oans {
inline std::string mac_post_overpass(const char* host,const std::string& body) {
    struct Runtime {
        Runtime() {if(curl_global_init(CURL_GLOBAL_DEFAULT)!=CURLE_OK)throw std::runtime_error("Cannot initialize HTTPS");}
        ~Runtime() {curl_global_cleanup();}
    };
    // Balance our libcurl reference when this aircraft plugin is unloaded.
    static Runtime runtime;
    std::unique_ptr<CURL,decltype(&curl_easy_cleanup)> request(curl_easy_init(),curl_easy_cleanup);
    if(!request)throw std::runtime_error("Cannot create HTTPS request");
    struct Response {std::string text;bool overflow=false,failed=false;} response;
    auto write=+[](char* data,size_t size,size_t count,void* user)->size_t {
        auto& result=*static_cast<Response*>(user);
        constexpr size_t limit=32u*1024u*1024u;
        if(size&&count>(limit-result.text.size())/size) {result.overflow=true;return 0;}
        const auto bytes=size*count;
        try {result.text.append(data,bytes);return bytes;}catch(...) {result.failed=true;return 0;}
    };
    const std::string url=std::string("https://")+host+"/api/interpreter";
    char error[CURL_ERROR_SIZE]={};auto* handle=request.get();
    curl_easy_setopt(handle,CURLOPT_URL,url.c_str());
    curl_easy_setopt(handle,CURLOPT_USERAGENT,"ToLissOANS/0.4.36 airport-map");
    curl_easy_setopt(handle,CURLOPT_POSTFIELDS,body.c_str());
    curl_easy_setopt(handle,CURLOPT_POSTFIELDSIZE_LARGE,static_cast<curl_off_t>(body.size()));
    curl_easy_setopt(handle,CURLOPT_NOSIGNAL,1L);
    curl_easy_setopt(handle,CURLOPT_CONNECTTIMEOUT,10L);
    curl_easy_setopt(handle,CURLOPT_TIMEOUT,85L);
    curl_easy_setopt(handle,CURLOPT_LOW_SPEED_LIMIT,1L);
    curl_easy_setopt(handle,CURLOPT_LOW_SPEED_TIME,45L);
    curl_easy_setopt(handle,CURLOPT_ERRORBUFFER,error);
    curl_easy_setopt(handle,CURLOPT_WRITEFUNCTION,write);
    curl_easy_setopt(handle,CURLOPT_WRITEDATA,&response);
    const auto result=curl_easy_perform(handle);
    if(response.overflow)throw std::runtime_error("Overpass response exceeds 32 MiB");
    if(response.failed)throw std::runtime_error("Cannot store Overpass response");
    if(result!=CURLE_OK)throw std::runtime_error(std::string("Overpass HTTPS: ")+(error[0]?error:curl_easy_strerror(result)));
    long status=0;curl_easy_getinfo(handle,CURLINFO_RESPONSE_CODE,&status);
    if(status<200||status>=300)throw std::runtime_error("Overpass HTTP status "+std::to_string(status));
    return response.text;
}
}
