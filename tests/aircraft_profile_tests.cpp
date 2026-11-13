#include "aircraft_profile.hpp"
#include "test_support.hpp"
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
int legacy(int argc,char** argv){try{
 check(argc<=2,"Supply at most one fresh fixture directory");
 Path root=argc==2?Path(argv[1]):std::filesystem::temp_directory_path()/("oans-legacy-"+std::to_string(std::chrono::high_resolution_clock::now().time_since_epoch().count()));
 check(!std::filesystem::exists(root),"Use fresh directory");
 int passed=0;
 auto run=[&](const std::string& name,bool separate,bool modern,const std::string& variant,bool success){
  scenario("profile/"+name);
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
 run("modern",false,true,"",true);
 run("ambiguous-batch",false,false,"duplicate",false);
 run("missing-brightness",true,false,"brightness",false);
 run("invalid-indices",true,false,"bad-index",false);
 run("incomplete-modern",false,true,"partial",false);
 std::cout<<"PASS "<<passed<<" legacy/modern generation and fail-closed cases\n";
 if(argc==1)std::filesystem::remove_all(root);return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}

#include "aircraft/lr/display.hpp"
#include "aircraft/zibo/display.hpp"
#include "aircraft/lr/identity.hpp"
#include "aircraft/zibo/identity.hpp"
#include "aircraft/magknight/identity.hpp"
#include "aircraft/magknight/display.hpp"
#include <map>
int main(int argc,char** argv){try{
 if(legacy(1,argv))return 1;
 const auto root=Path("build/profile-pruned");std::filesystem::create_directories(root);
 const auto source=Path("config/toliss");std::string error;
 std::ifstream in(source/"a20n.json");const std::string good((std::istreambuf_iterator<char>(in)),{});
 auto rejected=[&](const std::string& text){
  verify(profile::load(source,"A20N",error),error);write(root/"a20n.json",text);
  verify(!profile::load(root,"A20N",error)&&!error.empty()&&profile::current.nd_uv[0][0][0]==0,"invalid profile must clear previously loaded geometry");
 };
 scenario("profile/malformed-json-stale-reset");rejected(good+"garbage");
 scenario("profile/invalid-uv-range");auto invalid=good;invalid.replace(invalid.find("0.18650067"),10,"1.1");rejected(invalid);
 scenario("profile/path-traversal");verify(!profile::load(source,"../x",error),"ICAO cannot escape config directory");
 scenario("profile/id-precedence-fails-closed");
 const auto acf_path=root/"plane.acf";write(acf_path,acf());write(root/"objects/cockpit_INN.obj",obj(false));
 const auto mk=root/"renamed-aircraft.acf";
 auto mk_identity=[&](const std::string& icao,const std::string& name,const std::string& author="Magknight, RZSim, Heroes and Cows") {
  write(mk,"P acf/_ICAO "+icao+"\nP acf/_author "+author+" \r\nP acf/_name "+name+" \r\nPANEL_2D_BEGIN\n");
  return oans::magknight::identify(mk);
 };
 verify(mk_identity("B788","Magknight 787-8 Aviators XP12")=="B788","787-8 identity ignores renamed ACF filename");
 verify(mk_identity("B789","Magknight 787-9 Aviators XP12")=="B789","787-9 identity");
 verify(mk_identity("B789","Magknight 787-10 Aviators XP12")=="B78X","787-10 declaring B789 selects its own profile");
 verify(mk_identity("B78X","Magknight 787-10 Aviators XP12")=="B78X","787-10 with corrected ICAO");
 verify(mk_identity("B789","Magknight 787-8 Aviators XP12").empty(),"inconsistent identity fails closed");
 verify(mk_identity("B788","Magknight 787-8 Aviators XP12","Other").empty(),"other vendor must not match Magknight");
 verify(mk_identity("B789","Magknight 787-11 Aviators XP12").empty(),"uninspected variant is unsupported");
 verify(oans::magknight::identify(root/"missing.acf").empty(),"missing ACF fails closed");
 verify(profile::load("config/magknight","B788",error),error);
 const auto mk788=profile::current;
 for(const auto& id:{"B789","B78X"}) {
  verify(profile::load("config/magknight",id,error),error);
  const double dy=std::string(id)=="B78X"?-3.0480000179832:0;
  const double dz=std::string(id)=="B789"?-3.0477046692216:0;
  for(int side=0;side<2;++side)for(int corner=0;corner<4;++corner)for(int axis=0;axis<3;++axis)
   verify(std::abs(profile::current.nd_corners[side][corner][axis]-mk788.nd_corners[side][corner][axis]-(axis==1?dy:axis==2?dz:0))<1e-7,"variant-specific CG-relative click coordinates");
  for(int resolution=0;resolution<2;++resolution)for(int side=0;side<2;++side)for(int axis=0;axis<4;++axis)
   verify(profile::current.nd_uv[resolution][side][axis]==mk788.nd_uv[resolution][side][axis],"shared inboard screen UV layout");
  verify(std::abs(oans::magknight::display::aspect(profile::current.nd_corners[0],2)-oans::magknight::display::aspect(mk788.nd_corners[0],2))<1e-7,"CG translation preserves display proportions");
 }
 verify(profile::load("config/magknight","B788",error),error);
 verify(std::abs(profile::current.nd_uv[0][0][2]-.34472299)<1e-8,"B788 captain full display geometry");
 const auto* full=profile::current.nd_uv[0][0];
 const auto half=oans::magknight::display::uv(full,1);
 const auto whole=oans::magknight::display::uv(full,2);
 verify(std::abs(half[0]-.17229374)<1e-7&&whole[0]<.00002&&half[2]==whole[2],"B788 half uses right ND pane, full includes left CDU");
 verify(oans::magknight::display::uv(full,0)==half,"off switch stays in the ND pane");
 const auto left=oans::magknight::display::uv(full,1,false);
 verify(left[0]==whole[0]&&left[2]==half[0],"B788 half follows left ND pane after display switching");

 const double half_aspect=oans::magknight::display::aspect(profile::current.nd_corners[0],1);
 const double full_aspect=oans::magknight::display::aspect(profile::current.nd_corners[0],2);
 verify(half_aspect>.66&&half_aspect<.67&&std::abs(full_aspect-2*half_aspect)<1e-5,"B788 physical half/full aspect comes from 3-D glass dimensions");
 for(double aspect:{half_aspect,full_aspect}) {
  const double x=381;
  verify(std::abs(oans::magknight::display::map_x(300+(x-300)/aspect,aspect)-x)<1e-9,"B788 picking inverts physical aspect compensation");
 }
 const auto& corners=profile::current.nd_corners[0];
 const auto half_left=oans::magknight::display::corner(corners,0,1);
 verify(std::abs(half_left[0]+.225370575)<1e-7&&oans::magknight::display::corner(corners,1,1)==oans::magknight::display::corner(corners,1,2),"B788 click corners follow display extent");
 verify(oans::magknight::display::corner(corners,1,1,false)==half_left,"B788 left pane click projection shares the same seam");
 verify(oans::magknight::display::next_mode(0)==1&&oans::magknight::display::next_mode(1)==2&&oans::magknight::display::next_mode(2)==0,"B788 three-stage cycle wraps to off");
 write(root/"config/a339.json",good);std::string id;verify(oans::toliss::identify(acf_path,id,error),error);
 write(root/"config"/(id+".json"),"{}");Path loaded;
 verify(!oans::toliss::load_for_aircraft(root/"config",acf_path,"A339",loaded,error)&&loaded.empty(),"broken identity-specific profile cannot silently fall back to generic geometry");
 std::map<std::string,std::vector<double>> values;
 auto numeric=[&](const std::string& key,int i){auto it=values.find(key);return it!=values.end()&&i>=0&&i<int(it->second.size());};
 oans::aircraft::DataAccess data{[&](const std::string& key,int i,double fallback){return numeric(key,i)?values.at(key)[i]:fallback;},numeric};
 verify(profile::load("config/lr","A333",error),error);values["bright"]=std::vector<double>(32,1);
 for(int side=0;side<2;++side){
  for(const char* k:{"att","hdg","lrn"})values[oans::lr::display::key(k,side)]={1};
  for(const char* k:{"swap","ecam"})values[oans::lr::display::key(k,side)]={0};
  for(const char* k:{"mode","nd_state","pfd_state"})values[oans::lr::display::key(k,side)]={3};
  values[oans::lr::display::key("range",side)]={10};values[oans::lr::display::key("map",side)]={2};
 }
 auto lr=[&](int side){return oans::lr::display::overlay_blocked(data,side,false)[0]=='\0';};
 scenario("display/lr-range-side-isolation");verify(lr(0)&&lr(1),"LR aligned baseline");values["range0"][0]=40;
 verify(!lr(0)&&lr(1)&&oans::lr::display::advisory_available(data,0,true),"range blocks only own map, keeps PFD warning");values["range0"][0]=10;
 scenario("display/lr-power-isolation");values["pfd_state0"][0]=0;
 verify(!oans::lr::display::advisory_available(data,0,true)&&oans::lr::display::advisory_available(data,0,false)&&oans::lr::display::advisory_available(data,1,true),"PFD power cannot affect ND or opposite PFD");values["pfd_state0"][0]=3;
 scenario("display/lr-navigation-validity");values["lrn0"][0]=0;verify(!lr(0)&&lr(1),"invalid navigation blocks own overlay");values["lrn0"][0]=1;
 scenario("display/lr-missing-interface");values.erase("swap0");verify(!oans::lr::display::ready(data),"missing switching interface fails closed");
 verify(profile::load("config/zibo","B738",error),error);values.clear();values["bright"]=std::vector<double>(32,1);
 for(int side=0;side<2;++side){
  for(const char* k:{"range","vsd","du","pfd_du","irs"})values[oans::zibo::display::key(k,side)]={0};
  values[oans::zibo::display::key("mode",side)]={2};values[oans::zibo::display::key("expanded",side)]={1};
 }
 auto zibo=[&](int side){return oans::zibo::display::overlay_blocked(data,side,true)[0]=='\0';};
 scenario("display/zibo-range-side-isolation");verify(zibo(0)&&zibo(1),"Zibo 5 NM baseline");values["range0"][0]=1;
 verify(!zibo(0)&&zibo(1)&&oans::zibo::display::advisory_available(data,0,true),"test cannot bypass map range; PFD warning remains");values["range0"][0]=0;
 scenario("display/zibo-power-isolation");values["pfd_du0"][0]=-1;
 verify(!oans::zibo::display::advisory_available(data,0,true)&&oans::zibo::display::advisory_available(data,0,false)&&oans::zibo::display::advisory_available(data,1,true),"PFD power blocks only its screen");values["pfd_du0"][0]=0;
 scenario("display/zibo-navigation-validity");values["irs0"][0]=1;verify(!zibo(0)&&zibo(1),"alignment blocks own overlay");values["irs0"][0]=0;
 scenario("display/zibo-missing-interface");values.erase("range0");verify(!oans::zibo::display::ready(data)&&!zibo(0),"missing range interface fails closed");
 return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
