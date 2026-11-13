#pragma once
#include <array>
#include <cmath>

namespace oans::magknight::display {
// Calibrated in Test's positive-down Y convention; session tuning is additive.
inline constexpr int standard_y_offset=40;
// Both OANS layouts cover the complete selected display area.
inline constexpr double half_overlay_top=600;
// Inset the half-pane right edge by 1% of its width to clear the bezel.
inline constexpr double half_right_inset=0.01;
// The captain's inboard MFD has two vertical panes. Half OANS occupies the
// active ND pane; full OANS merges both. PFD and lower CDU are outside it.
inline int next_mode(int mode) {return (mode+1)%3;} // off, ND pane, full display
inline std::array<double,4> uv(const double* full,int mode,bool right=true) {
    std::array<double,4> result{full[0],full[1],full[2],full[3]};
    // Authored UV seam, slightly left of the geometric midpoint.
    if(mode!=2) {
        const double seam=full[0]+(full[2]-full[0])*.4997874021940581;
        if(right) {result[0]=seam;result[2]-=(full[2]-seam)*half_right_inset;}
        else result[2]=seam-(seam-full[0])*half_right_inset;
    }
    return result;
}
inline std::array<double,3> corner(const double full[4][3],int index,int mode,bool right=true) {
    std::array<double,3> result{full[index][0],full[index][1],full[index][2]};
    if(mode!=2) {
        const bool left_corner=index==0||index==3;
        const int opposite=index==0?1:index==1?0:index==2?3:2;
        // The original half pane spans [0,.5] or [.5,1].
        const double fraction=left_corner?(right?.5:0):(right?1-half_right_inset*.5:.5*(1-half_right_inset));
        for(int axis=0;axis<3;++axis) {
            const double left=left_corner?full[index][axis]:full[opposite][axis];
            const double right_edge=left_corner?full[opposite][axis]:full[index][axis];
            result[axis]=left+(right_edge-left)*fraction;
        }
    }
    return result;
}
inline double aspect(const double full[4][3],int mode) {
    const auto a=corner(full,0,mode),b=corner(full,1,mode),c=corner(full,3,mode);
    const auto distance=[](const auto& p,const auto& q){return std::sqrt(
        (p[0]-q[0])*(p[0]-q[0])+(p[1]-q[1])*(p[1]-q[1])+(p[2]-q[2])*(p[2]-q[2]));};
    const double w=distance(a,b),h=distance(a,c);
    return w>0&&h>0?w/h:1;
}
inline double map_x(double x,double aspect) {return 300+(x-300)*aspect;}
}
