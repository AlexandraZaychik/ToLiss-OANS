#pragma once
#ifdef _WIN32
#include <windows.h>
#endif
#include <string>
#include <vector>
#include <stdexcept>

namespace oans {
// Shared by the plugin and offline preview. Top-down BGRA, rendered at 2x.
constexpr int about_width=560,about_height=460,about_scale=2;
#ifdef __APPLE__
std::vector<unsigned char> raster_about_panel(const std::string& version);
#else
inline std::vector<unsigned char> raster_about_panel(const std::string& version) {
    const int width=about_width*about_scale,height=about_height*about_scale;
    HDC dc=CreateCompatibleDC(nullptr);
    if(!dc)throw std::runtime_error("About: cannot create text surface");
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
    info.bmiHeader.biWidth=width;info.bmiHeader.biHeight=-height;
    info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
    void* bits=nullptr;HBITMAP bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&bits,nullptr,0);
    if(!bitmap) {DeleteDC(dc);throw std::runtime_error("About: cannot create bitmap");}
    auto old_bitmap=SelectObject(dc,bitmap);
    auto fill=[&](int x,int y,int w,int h,COLORREF color) {
        RECT box{x*2,y*2,(x+w)*2,(y+h)*2};HBRUSH brush=CreateSolidBrush(color);
        FillRect(dc,&box,brush);DeleteObject(brush);
    };
    auto label=[&](int x,int y,const std::wstring& value,int size,COLORREF color,int weight=FW_NORMAL) {
        HFONT font=CreateFontW(-size*2,0,0,0,weight,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,ANTIALIASED_QUALITY,DEFAULT_PITCH,L"Microsoft YaHei UI");
        auto old_font=SelectObject(dc,font);SetTextColor(dc,color);SetBkMode(dc,TRANSPARENT);
        TextOutW(dc,x*2,y*2,value.c_str(),static_cast<int>(value.size()));
        SelectObject(dc,old_font);DeleteObject(font);
    };
    const COLORREF ink=RGB(233,239,246),muted=RGB(146,160,180),mint=RGB(125,228,194);
    fill(0,0,560,460,RGB(19,26,37));
    fill(32,34,28,3,mint);
    label(32,51,L"ToLiss OANS",32,ink,FW_SEMIBOLD);
    label(433,67,L"v"+std::wstring(version.begin(),version.end()),13,muted);
    label(34,105,L"Eyderoe (GPT)",16,ink);
    label(34,133,L"GPLv3  /  Open source",13,muted);
    fill(32,178,496,83,RGB(28,49,52));
    fill(32,178,3,83,mint);
    label(52,190,L"FREE PLUGIN",22,mint,FW_SEMIBOLD);
    label(53,225,L"\u514d\u8d39\u63d2\u4ef6",17,RGB(193,223,214));
    label(34,283,L"You might find it on GitHub.",14,muted);
    fill(32,326,496,1,RGB(47,58,74));
    label(34,343,L"THANKS / \u81f4\u8c22",12,muted,FW_SEMIBOLD);
    label(34,373,L"CHOXON2CHOXOFF",15,ink,FW_SEMIBOLD);
    label(227,375,L"(for awesome video)",13,muted);
    label(34,407,L"KILO_333167",15,ink,FW_SEMIBOLD);
    label(227,409,L"(for other plane config file)",13,muted);
    GdiFlush();
    const auto* data=static_cast<const unsigned char*>(bits);
    std::vector<unsigned char> pixels(data,data+width*height*4);
    for(size_t i=3;i<pixels.size();i+=4)pixels[i]=255;
    SelectObject(dc,old_bitmap);DeleteObject(bitmap);DeleteDC(dc);
    return pixels;
}
#endif
}
