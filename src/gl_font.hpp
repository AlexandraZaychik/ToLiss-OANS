#pragma once
#include "gl_state.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <map>
#include <algorithm>
#include <cmath>

namespace oans {
// Native reader for the licensed B612 Mono atlas supplied by A350 BTV.
// Metrics are baseline/advance aware; only our own texture is created or used.
class AtlasFont {
    struct Size {int cap=0,advance=0,w=0,h=0,base=0,left=0,x=0,y=0,cols=0;};
    std::map<int,Size> sizes_;
    std::vector<unsigned char> alpha_;
    int width_=0,height_=0,id_=0;
    HGLRC context_=nullptr;
    const Size& pick(double cap) const {
        auto i=sizes_.lower_bound(static_cast<int>(std::round(cap)));
        if(i==sizes_.end())--i;
        return i->second;
    }
public:
    bool load(const std::filesystem::path& path) {
        std::ifstream f(path,std::ios::binary);std::string row,tag;int ns=0,first=0,count=0;double u,v,aspect;
        if(!std::getline(f,row))return false;
        std::istringstream head(row);head>>tag>>width_>>height_>>ns>>first>>count>>u>>v>>aspect;
        if(tag!="BTVFONT2"||width_<=0||height_<=0||width_>4096||height_>4096||ns<1||ns>64||first!=32||count<95)return false;
        sizes_.clear();
        for(int k=0;k<ns;++k) {
            if(!std::getline(f,row))return false;
            Size s;std::istringstream item(row);item>>tag>>s.cap>>s.advance>>s.w>>s.h>>s.base>>s.left>>s.x>>s.y>>s.cols;
            if(!item||tag!="S"||s.cols<=0||s.w<=0||s.h<=0)return false;
            if(s.cap<100)sizes_[s.cap]=s;
        }
        if(!std::getline(f,row)||row!="DATA"||sizes_.empty())return false;
        alpha_.resize(static_cast<size_t>(width_)*height_);f.read(reinterpret_cast<char*>(alpha_.data()),alpha_.size());
        return f.gcount()==static_cast<std::streamsize>(alpha_.size());
    }
    bool ready() const {return !sizes_.empty()&&(!alpha_.empty()||id_);}
    bool upload(void(*allocate)(int*,int),void(*bind)(int,int)) {
        if(id_)return true;
        if(!ready()||!wglGetCurrentContext())return false;
        GLint unpack=0;glGetIntegerv(0x88EF,&unpack);
        using BindBuffer=void(APIENTRY*)(GLenum,GLuint);auto bind_buffer=gl_proc<BindBuffer>("glBindBuffer");
        if(unpack&&!bind_buffer)return false;
        if(unpack)bind_buffer(0x88EC,0);
        glPushClientAttrib(GL_CLIENT_PIXEL_STORE_BIT);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
        glPixelStorei(GL_UNPACK_ROW_LENGTH,0);glPixelStorei(GL_UNPACK_SKIP_ROWS,0);glPixelStorei(GL_UNPACK_SKIP_PIXELS,0);
        allocate(&id_,1);bind(id_,0);
        std::vector<unsigned char> rgba(alpha_.size()*4,255);
        for(size_t i=0;i<alpha_.size();++i)rgba[i*4+3]=alpha_[i];
        glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width_,height_,0,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,0x812F);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,0x812F);
        glPopClientAttrib();if(unpack)bind_buffer(0x88EC,unpack);
        context_=wglGetCurrentContext();alpha_.clear();alpha_.shrink_to_fit();return id_!=0;
    }
    int texture() const {return id_;}
    double width(const std::string& s,double cap=18) const {if(sizes_.empty())return 0;const auto& z=pick(cap);return s.size()*z.advance*cap/z.cap;}
    bool draw(double x,double y,const std::string& s,const float* rgb,double max_width,double cap=18,bool ellipsis=true) {
        if(!id_)return false;
        const auto& z=pick(cap);double k=cap/z.cap,advance=z.advance*k;
        size_t count=std::min(s.size(),static_cast<size_t>(std::max(0.0,std::floor((max_width+.001)/advance))));
        std::string shown=s.substr(0,count);if(ellipsis&&count<s.size()&&count>=3)shown.replace(count-3,3,"...");
        GLint env;glGetTexEnviv(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,&env);glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,GL_MODULATE);
        glColor3fv(rgb);glBegin(GL_QUADS);
        for(unsigned char ch:shown) {
            int i=(ch>=32&&ch<=126)?ch-32:31;
            double u0=double(z.x+(i%z.cols)*z.w)/width_,v0=double(z.y+(i/z.cols)*z.h)/height_;
            double u1=u0+double(z.w)/width_,v1=v0+double(z.h)/height_;
            double l=x-z.left*k,r=l+z.w*k,t=y+z.base*k,b=t-z.h*k;
            glTexCoord2d(u0,v0);glVertex2d(l,t);glTexCoord2d(u1,v0);glVertex2d(r,t);
            glTexCoord2d(u1,v1);glVertex2d(r,b);glTexCoord2d(u0,v1);glVertex2d(l,b);x+=advance;
        }
        glEnd();glTexEnvi(GL_TEXTURE_ENV,GL_TEXTURE_ENV_MODE,env);return true;
    }
    void release() {if(id_&&wglGetCurrentContext()==context_){GLuint id=id_;glDeleteTextures(1,&id);}id_=0;context_=nullptr;}
};
}
