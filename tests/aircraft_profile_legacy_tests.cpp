#include "aircraft_profile.hpp"
#include "aircraft/toliss/profile.hpp"
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>
using Path=std::filesystem::path;
void check(bool ok,const std::string& why){if(!ok)throw std::runtime_error(why);}
void write(const Path& p,const std::string& text){std::filesystem::create_directories(p.parent_path());std::ofstream f(p);f<<text;check(bool(f),"write fixture");}
std::string acf(){return "P acf/_ICAO A339\nP acf/_cgY 0\nP acf/_cgZ 0\nP _obja/0/_v10_att_file_stl cockpit_INN.obj\nP _obja/0/_v10_att_phi_ref 0\nP _obja/0/_v10_att_psi_ref 0\nP _obja/0/_v10_att_the_ref 0\nP _obja/0/_v10_att_x_acf_prt_ref 0\nP _obja/0/_v10_att_y_acf_prt_ref 0\nP _obja/0/_v10_att_z_acf_prt_ref 0\n";}
std::string obj(bool separate,bool modern=false,const std::string& variant=""){
 std::ostringstream out;
 const double x[]={-.6,-.35,.6,.35,0,0};
 // Different UVs in each slot prove the modern selector remains intact.
 const int modes=modern?2:1;
 for(int mode=0;mode<modes;++mode)for(int screen=0;screen<6;++screen){
  const double u=.02+screen*.15,v=.2+mode*.3,y=screen==5?.7:1;
  for(auto corner:std::array<std::array<int,2>,4>{{{0,1},{1,1},{1,0},{0,0}}})
   out<<"VT "<<x[screen]+corner[0]*.16<<' '<<y+corner[1]*.16<<" 0 0 0 1 "<<u+corner[0]*.12<<' '<<v+corner[1]*.12<<'\n';
 }
 for(int i=0;i<modes*6;++i)for(int c:{0,1,2,0,2,3})out<<"IDX "<<i*4+c<<'\n';
 out<<"ATTR_cockpit\nATTR_manip_none\n";
 if(modern){
  for(int mode=0;mode<2;++mode){
   if(variant=="partial"&&mode==1)continue;
   out<<"ANIM_begin\nANIM_hide "<<(1-mode)<<' '<<(1-mode)<<" AirbusFBW/DisplayResolutionFallback\nTRIS "<<mode*36<<" 36\nANIM_end\n";
  }
 }else if(separate){
  for(int screen=0;screen<6;++screen){
   out<<"ATTR_manip_command hand AirbusFBW/PopUp"<<(screen==1?"ND1":screen==3?"ND2":"DU")<<"\nTRIS "<<screen*6<<" 6\n";
  }
 }else out<<"TRIS 0 36\n";
 // Animated, non-panel and unrelated manipulator quads must not be collected.
 out<<"ATTR_manip_none\nANIM_begin\nTRIS 0 6\nANIM_end\nATTR_no_cockpit\nTRIS 0 6\nATTR_cockpit\nATTR_manip_command hand other/command\nTRIS 0 6\n";
 if(variant=="duplicate")out<<"ATTR_manip_none\nTRIS 0 6\n";
 if(variant=="bad-index")out<<"TRIS 999999 6\n";
 for(int i=0;i<4;++i){
  if(variant=="brightness"&&i==3)continue;
  // Named animation anchors for the modern path only.
  if(modern&&(i==1||i==3))out<<"# ckpt/lights/ndInner"<<(i==1?"Left":"Right")<<"/anim\n";
  out<<"ATTR_manip_drag_axis rotate_medium 0.06 0 0 0 1 AirbusFBW/DUBrightness["<<i<<"]\n";
 }
 return out.str();
}
int main(int argc,char** argv){try{
 check(argc<=2,"Supply at most one fresh fixture directory");
 Path root=argc==2?Path(argv[1]):std::filesystem::temp_directory_path()/("oans-legacy-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
 check(!std::filesystem::exists(root),"Use fresh directory");
 int passed=0;
 auto run=[&](const std::string& name,bool separate,bool modern,const std::string& variant,bool success){
  auto dir=root/name;write(dir/"plane.acf",acf());write(dir/"objects/cockpit_INN.obj",obj(separate,modern,variant));
  std::filesystem::path loaded;std::string error;
  const bool ok=oans::toliss::load_for_aircraft(dir/"config",dir/"plane.acf","A339",loaded,error);
  check(ok==success,name+": "+error);
  if(success){
   auto p=profile::current;
   check(std::abs(p.nd_uv[0][0][0]-.17)<1e-10,"Captain ND identification");
   check(std::abs(p.nd_uv[0][1][0]-.47)<1e-10,"FO ND identification");
   check(std::abs(p.pfd_uv[0][0][0]-.02)<1e-10&&std::abs(p.pfd_uv[0][1][0]-.32)<1e-10,"PFD identification");
   check(p.nd_brightness[0]==1&&p.nd_brightness[1]==3,"Unnamed legacy brightness channels");
   check(std::abs(p.nd_uv[1][0][1]-(modern?.5:.2))<1e-10,"Resolution slots");
   check(std::abs(p.nd_corners[0][0][0]+.35)<1e-10&&std::abs(p.nd_corners[1][0][0]-.35)<1e-10,"Click corners");
  }else check(loaded.empty()&&!std::filesystem::exists(dir/"config"),"Failure published a profile");
  ++passed;
 };
 run("static-batch",false,false,"",true);
 run("static-separate",true,false,"",true);
 run("modern",false,true,"",true);
 run("ambiguous-batch",false,false,"duplicate",false);
 run("ambiguous-separate",true,false,"duplicate",false);
 run("missing-brightness",true,false,"brightness",false);
 run("invalid-indices",true,false,"bad-index",false);
 run("incomplete-modern",false,true,"partial",false);
 std::cout<<"PASS "<<passed<<" legacy/modern generation and fail-closed cases\n";
 if(argc==1)std::filesystem::remove_all(root);
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
