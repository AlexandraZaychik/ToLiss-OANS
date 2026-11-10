// The runner extracts gauge_draw and PanelTrace from current production source.
// Stub only the surrounding SDK/GL interfaces; never compile or load the plugin.
#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
using GLdouble=double;using GLint=int;using XPLMDrawingPhase=int;
enum {GL_MODELVIEW_MATRIX,GL_PROJECTION_MATRIX,GL_VIEWPORT,GL_POLYGON_MODE,GL_CULL_FACE,GL_COLOR_LOGIC_OP,GL_STENCIL_TEST,GL_SCISSOR_TEST,GL_SCISSOR_BOX,GL_VENDOR,GL_RENDERER,GL_VERSION};
#define OANS_TRACE_SCOPE() ((void)0)
#if OANS_DEBUG
#define OANS_TRACE(message) ((void)(message))
#else
#define OANS_TRACE(...) ((void)0)
#endif
#include "gauge_trace_under_test.hpp"
PanelTrace panel_trace[3];std::uint64_t panel_trace_generation=1;
bool enabled=true,nd_on[2]={true,true},map_text=false,profile_ok=true,aircraft_ok=true;
double now=0,heartbeat=0,render_gain=1;int pass=2;unsigned pass_count[3]={};
struct {bool nd=true;} config;
std::string status;
bool overlays[2]={true,true},warning_nd[2]={},warning_pfd[2]={};
int overlay_checks=0,warning_checks=0,queries=0,matrix_queries=0,diagnostic_queries=0;
int state_scopes=0,nd_draws=0,clears=0,nd_warnings=0,pfd_warnings=0,projections=0,texture_calls=0,bounds_reads=0;
GLint viewport[4]={0,0,4096,4096};double model_width=4096;
double last_draw_width=0;
double XPLMGetElapsedTime(){return now;}
double value(const char* key){std::string k=key;if(k=="rt")return pass;++bounds_reads;return 0;}
const char* nd_overlay_blocked(int side){++overlay_checks;return overlays[side]?"":"BLOCKED";}
bool advisory_display_available(int side,bool pfd){++warning_checks;return pfd?warning_pfd[side]:warning_nd[side];}
void glGetDoublev(int,GLdouble* out){++queries;++matrix_queries;std::fill(out,out+16,0.);out[0]=model_width;}
void glGetIntegerv(int key,GLint* out){++queries;if(key==GL_VIEWPORT)std::copy(viewport,viewport+4,out);else {++diagnostic_queries;*out=0;}}
bool glIsEnabled(int){++queries;++diagnostic_queries;return false;}
const unsigned char* glGetString(int){++queries;return reinterpret_cast<const unsigned char*>("mock");}
void gluProject(double,double,double,const GLdouble*,const GLdouble*,const GLint*,double* x,double* y,double* z){++projections;*x=*y=*z=0;}
struct TextureRect{double l,b,w,h;};
std::optional<TextureRect> texture_rectangle(const GLdouble* model,const GLdouble*,const GLint* view){++texture_calls;if(view[2]<=0)return {};return TextureRect{0,0,model[0],model[0]};}
void graphics(){}void restore_graphics(const int*,int){}
struct DrawingState {DrawingState(void(*)(),void(*)(const int*,int)){++state_scopes;}};
struct Backend {
 const double* uv(int,int,bool) const {static const double v[]={0,0,1,1};return v;}
 double brightness(int,int,bool) const{return .5;}
} backend;
Backend* aircraft_backend=&backend;int aircraft_data=0;
void rect(double,double,double,double,const float*){++clears;}
void render_rect(double,double,double w,double,int side,bool){++nd_draws;last_draw_width=w;if(warning_nd[side])++nd_warnings;}
void render_advisory_rect(double,double,double w,double,int,bool pfd){last_draw_width=w;if(pfd)++pfd_warnings;else ++nd_warnings;}
void log(const std::string&){}void display_log(const std::string&){}
#include "gauge_draw_under_test.hpp"
void reset(){
 for(auto& t:panel_trace)t=PanelTrace{};
 panel_trace_generation=1;enabled=true;config.nd=true;
 overlays[0]=overlays[1]=true;nd_on[0]=nd_on[1]=true;
 warning_nd[0]=warning_nd[1]=warning_pfd[0]=warning_pfd[1]=false;
 now=heartbeat=0;pass=2;viewport[2]=viewport[3]=4096;model_width=4096;
 queries=matrix_queries=diagnostic_queries=state_scopes=nd_draws=clears=nd_warnings=pfd_warnings=projections=texture_calls=bounds_reads=overlay_checks=warning_checks=0;
}
void draw(double time=0){now=heartbeat=time;gauge_draw(0,0,nullptr);}
int checks=0;void check(bool value,const char* message){++checks;if(!value)throw std::runtime_error(message);}
int main(){try {
 reset();overlays[0]=overlays[1]=false;draw();
 check(queries==0&&state_scopes==0&&texture_calls==0&&bounds_reads==0,"idle callback must do no GL/transform/bounds work");
 ++panel_trace_generation;draw(.1);check(queries==0&&panel_trace[2].sample_generation==0,"idle diagnostic request stays pending without GL");
 overlays[0]=true;draw(.2);check(panel_trace[2].sample_generation==panel_trace_generation,"pending request sampled when overlay becomes drawable");
 reset();nd_on[0]=nd_on[1]=false;pass=1;draw();
 check(queries==0&&clears==0&&state_scopes==0,"unchecked maps have no clear-pass work");
 warning_pfd[0]=true;draw();check(queries==0,"PFD warning must not activate clear pass");
 pass=2;draw();check(nd_draws==2&&pfd_warnings==1,"final pass preserves unchecked checkboxes and PFD warning");
 reset();overlays[0]=overlays[1]=false;warning_nd[0]=true;warning_pfd[1]=true;draw();
 check(nd_draws==0&&nd_warnings==1&&pfd_warnings==1&&state_scopes==1,"independent warnings survive unavailable overlays");
 reset();warning_nd[0]=true;draw();check(nd_warnings==1,"overlay owns ND warning, no duplicate independent draw");
 check(overlay_checks==2&&warning_checks==4,"evaluate each side's display gates once");
 check(matrix_queries==2&&texture_calls==1&&diagnostic_queries==10,"single transform snapshot and initial diagnostic sample");
 check(projections==4&&bounds_reads==4,"sample projected diagnostics once per side");
 const int q=queries,diag=diagnostic_queries,project=projections,bounds=bounds_reads;
 model_width=8192;draw(.1);
 check(queries-q==3&&matrix_queries==4&&texture_calls==2,"unsampled active callback still reads fresh transform");
 check(last_draw_width==8192&&nd_draws==4,"live target changes apply before diagnostic timer expires");
 check(diagnostic_queries==diag&&projections==project&&bounds_reads==bounds,"diagnostic work throttled between samples");
 check(panel_trace[2].sampled_at==0&&panel_trace[2].time==.1,"sample time distinct from callback time");
 draw(1);check(diagnostic_queries==20&&panel_trace[2].sampled_at==1,"one-second refresh");
 ++panel_trace_generation;draw(1.1);check(diagnostic_queries==30&&panel_trace[2].sample_generation==panel_trace_generation,"explicit report request refreshes inside next drawable callback");
 draw(.2);check(diagnostic_queries==40&&panel_trace[2].sampled_at==.2,"clock rewind refreshes diagnostics");
 pass=1;draw(.3);check(panel_trace[1].sampled_at==.3&&clears==2,"clear pass has independent sampling and actual clears");
 reset();draw();const int scopes=state_scopes;viewport[2]=20;draw(.1);
 check(state_scopes==scopes&&nd_draws==2,"reject current invalid viewport even with cached diagnostics");
 draw(1.2);check(panel_trace[2].target[2]==0&&panel_trace[2].projected[0][2]==0,"invalid sampled transform does not retain stale diagnostic geometry");
 reset();enabled=false;draw();check(queries==0&&overlay_checks==0,"disabled plugin returns before gates and GL");
 reset();now=2;heartbeat=0;gauge_draw(0,0,nullptr);check(queries==0&&overlay_checks==0,"stale heartbeat returns before GL");
 reset();pass=0;draw();check(queries==0&&overlay_checks==0,"unrelated drawing pass is cheap");
 check(config.nd,"callback has no unexpected exception");
 std::cout<<"PASS "<<checks<<" checks against extracted production gauge_draw (OANS_DEBUG="<<OANS_DEBUG<<")\n";
 }catch(const std::exception& e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}}
