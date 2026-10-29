#import <AppKit/AppKit.h>
#include "about_panel.hpp"
#include "mac_platform.hpp"

namespace oans {
std::string clipboard_text() {
    @autoreleasepool {
        NSString* text=[[NSPasteboard generalPasteboard] stringForType:NSPasteboardTypeString];
        return text ? std::string([text UTF8String]) : std::string{};
    }
}

std::vector<unsigned char> raster_about_panel(const std::string& version) {
    @autoreleasepool {
        const int width=about_width*about_scale,height=about_height*about_scale;
        NSBitmapImageRep* bitmap=[[NSBitmapImageRep alloc]
            initWithBitmapDataPlanes:nullptr pixelsWide:width pixelsHigh:height
            bitsPerSample:8 samplesPerPixel:4 hasAlpha:YES isPlanar:NO
            colorSpaceName:NSDeviceRGBColorSpace bitmapFormat:static_cast<NSBitmapFormat>(0)
            bytesPerRow:width*4 bitsPerPixel:32];
        if(!bitmap)throw std::runtime_error("About: cannot create bitmap");
        NSGraphicsContext* context=[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap];
        if(!context)throw std::runtime_error("About: cannot create text surface");
        context=[NSGraphicsContext graphicsContextWithCGContext:[context CGContext] flipped:YES];
        [NSGraphicsContext saveGraphicsState];
        [NSGraphicsContext setCurrentContext:context];
        // Match the Windows top-down layout, with typography in logical pixels.
        NSAffineTransform* transform=[NSAffineTransform transform];
        [transform translateXBy:0 yBy:height];
        [transform scaleXBy:about_scale yBy:-about_scale];
        [transform concat];
        auto color=[](int r,int g,int b) {return [NSColor colorWithSRGBRed:r/255.0 green:g/255.0 blue:b/255.0 alpha:1];};
        auto fill=[&](int x,int y,int w,int h,NSColor* ink) {
            [ink setFill];NSRectFill(NSMakeRect(x,y,w,h));
        };
        auto label=[&](int x,int y,NSString* value,int size,NSColor* ink,bool bold=false) {
            NSFont* font=[NSFont systemFontOfSize:size weight:bold?NSFontWeightSemibold:NSFontWeightRegular];
            [value drawWithRect:NSMakeRect(x,y,about_width-x,60)
                options:NSStringDrawingUsesLineFragmentOrigin
                attributes:@{NSFontAttributeName:font,NSForegroundColorAttributeName:ink}
                context:nil];
        };
        NSColor* ink=color(233,239,246);NSColor* muted=color(146,160,180);NSColor* mint=color(125,228,194);
        fill(0,0,about_width,about_height,color(19,26,37));fill(32,34,28,3,mint);
        label(32,51,@"ToLiss OANS",32,ink,true);
        label(433,67,[NSString stringWithFormat:@"v%s",version.c_str()],13,muted);
        label(34,105,@"Eyderoe、GPT*",16,ink);label(34,133,@"GPLv3  /  Open source",13,muted);
        fill(32,178,496,83,color(28,49,52));fill(32,178,3,83,mint);
        label(52,190,@"FREE PLUGIN",22,mint,true);label(53,225,@"免费插件",17,color(193,223,214));
        label(34,283,@"You might find it on GitHub.",14,muted);fill(32,326,496,1,color(47,58,74));
        label(34,343,@"THANKS / 致谢",12,muted,true);
        label(34,373,@"CHOXON2CHOXOFF",15,ink,true);label(227,375,@"(for awesome video)",13,muted);
        label(34,407,@"KILO_333167",15,ink,true);label(227,409,@"(for other plane config file)",13,muted);
        label(34,435,@"heart4sck",15,ink,true);label(227,437,@"(for test on mac)",13,muted);
        label(34,469,@"ricardo404",15,ink,true);label(227,471,@"(for improve)",13,muted);
        [context flushGraphics];[NSGraphicsContext restoreGraphicsState];
        const unsigned char* data=[bitmap bitmapData];
        std::vector<unsigned char> pixels(data,data+width*height*4);
        // The shared uploader expects top-down BGRA, including on macOS.
        for(size_t i=0;i<pixels.size();i+=4) {std::swap(pixels[i],pixels[i+2]);pixels[i+3]=255;}
        return pixels;
    }
}
}
