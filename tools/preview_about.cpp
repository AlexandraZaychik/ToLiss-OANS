// Offline preview of the exact raster uploaded by the plugin. No X-Plane access.
#include "about_panel.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
int main(int argc,char** argv) {
    if(argc!=3){std::cerr<<"Usage: preview_about VERSION OUTPUT.bmp\n";return 2;}
    try {
        const auto pixels=oans::raster_about_panel(argv[1]);
        BITMAPFILEHEADER head{};head.bfType=0x4D42;
        head.bfOffBits=sizeof head+sizeof(BITMAPINFOHEADER);
        head.bfSize=head.bfOffBits+static_cast<DWORD>(pixels.size());
        BITMAPINFOHEADER info{};info.biSize=sizeof info;
        info.biWidth=oans::about_width*2;info.biHeight=-oans::about_height*2;
        info.biPlanes=1;info.biBitCount=32;info.biSizeImage=static_cast<DWORD>(pixels.size());
        std::ofstream out(std::filesystem::u8path(argv[2]),std::ios::binary);
        out.write(reinterpret_cast<const char*>(&head),sizeof head);
        out.write(reinterpret_cast<const char*>(&info),sizeof info);
        out.write(reinterpret_cast<const char*>(pixels.data()),pixels.size());
        if(!out)throw std::runtime_error("Cannot write preview");
        std::cout<<"Rendered shared About panel at 2x resolution\n";
    } catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
