#include <Arduino.h>
uint64_t fakeT=1000; double VPK_MV, IPK_MV, PHI, NOISE_SD, SPIKE_P;
#include "../../firmware/PowerScope/PowerScope.ino"
static void load(double amps,double pf,double sd){
  VPK_MV = 230/0.946*sqrt(2.0);
  IPK_MV = amps*sqrt(2.0)*(2.0/3.0*100.0);
  PHI = acos(pf); NOISE_SD=sd; SPIKE_P=0.004;
}
static std::string call(const char*url,int m,std::initializer_list<std::pair<const char*,const char*>> ps={}){
  AsyncWebServerRequest rq; for(auto&p:ps) rq.params[p.first].v=p.second;
  auto it=server.routes.find({url,m}); if(it==server.routes.end()) return "NO ROUTE";
  it->second(&rq); return rq.out.body;
}
static void settle(int n){ for(int k=0;k<n;k++){ handleRequests(); measure(); updateStats(); } }
static int fails=0;
static void check(bool ok,const char*what){ printf("  %s  %s\n",ok?"ok  ":"FAIL",what); if(!ok)fails++; }
static bool has(const std::string&s,const char*t){ return s.find(t)!=std::string::npos; }
static void run(const char* name,int n){
  double sr=0,si=0,sp=0,scf=0; int nonzero=0;
  for(int k=0;k<n;k++){ measure(); updateStats(); sr+=g_rawI; si+=M.I; sp+=M.P; scf+=M.cf; if(M.I>0)nonzero++; }
  printf("%-34s raw I %.3f | shown I %.3f | P %6.1f W | PF %.2f | windows showing a load: %d/%d | kWh %.6f\n",name,sr/n,si/n,sp/n,M.PF,nonzero,n,kWh);
}
int main(){
  srand(7); setup();
  printf("gate = %.3f A (noise %.3f x %.2f)\n",max(I_FLOOR,I_NOISE*GATE_FACTOR),I_NOISE,GATE_FACTOR);
  load(0,1,12.2); run("idle, quiet noise (0.19 A)",80);
  load(0,1,14.6); run("idle, burst noise (0.23 A)",80);
  load(0.39,0.82,12.2); run("air cooler 0.39 A, PF 0.82",60);
  load(2.0,1.0,12.2);   run("heater 2.0 A, PF 1.0",30);
  load(0.20,1.0,12.2);  run("small load 0.20 A (near the floor)",60);
  printf("--- zero calibration ---\n");
  load(0,1,12.2);   printf("no load      -> %s, I_NOISE now %.3f\n",calibrateZero()?"accepted":"refused",I_NOISE);
  load(2.0,1.0,12.2);printf("2 A load     -> %s, I_NOISE now %.3f\n",calibrateZero()?"accepted":"refused",I_NOISE);
  VPK_MV=0; IPK_MV=0; printf("no mains     -> %s, I_NOISE now %.3f\n",calibrateZero()?"accepted":"refused",I_NOISE);

  printf("--- calibration over HTTP ---\n");
  I_NOISE=I_NOISE_DEF;
  load(2.0,1.0,12.2); settle(10);
  printf("  heater before: V %.1f  I %.3f  P %.1f  PF %.3f\n",M.V,M.I,M.P,M.PF);
  check(has(call("/unlock",HTTP_POST,{{"pin","1234"}}),"\"ok\":1"),"default PIN 1234 unlocks");
  check(has(call("/unlock",HTTP_POST,{{"pin","0000"}}),"Wrong PIN"),"wrong PIN refused");
  check(has(call("/cal",HTTP_POST,{{"vgain","1.1"}}),"Wrong PIN"),"change without a PIN refused");
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"vref","241.5"}}),"\"ok\":1"),"multimeter 241.5 V accepted");
  settle(5); printf("  voltage now %.1f V (V_GAIN %.4f)\n",M.V,V_GAIN); check(fabs(M.V-241.5)<0.6,"voltage reads the multimeter value");
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"vref","40"}}),"\"ok\":0"),"silly multimeter value refused");
  float n0=I_NOISE;
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"iref","2.10"}}),"\"ok\":1"),"clamp 2.10 A accepted");
  settle(5); printf("  current now %.3f A (I_TRIM %.4f, I_NOISE %.3f)\n",M.I,I_TRIM,I_NOISE);
  check(fabs(M.I-2.10)<0.02,"current reads the clamp value"); check(fabs(I_NOISE/n0-I_TRIM)<1e-3,"idle noise rescaled with the trim");
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"shift","3"}}),"\"ok\":1"),"phase shift 3 accepted");
  settle(5); printf("  heater PF with shift 3: %.3f\n",M.PF); check(M.PF<0.995&&SHIFT==3,"shift moves the PF off 1.00");
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"shift","25"}}),"\"ok\":0"),"shift 25 refused");
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"isign","1"}}),"\"ok\":1"),"current direction flip accepted");
  settle(5); check(M.P<0,"flipped direction gives negative power");
  check(has(call("/cal",HTTP_GET),"\"isign\":1"),"GET /cal shows the new values");
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"defaults","1"}}),"\"ok\":1"),"restore defaults accepted");
  settle(5); check(V_GAIN==V_GAIN_DEF&&I_TRIM==I_TRIM_DEF&&SHIFT==SHIFT_DEF&&I_SIGN==I_SIGN_DEF&&I_NOISE==I_NOISE_DEF,"defaults restored");
  check(M.P>0&&fabs(M.V-230)<0.6,"heater reads 230 V and positive power again");
  load(0.2,1.0,12.2); settle(3);
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"iref","1.0"}}),"Connect a kettle"),"current calibration refused without a real load");
  check(has(call("/cal",HTTP_POST,{{"pin","1234"},{"vgain","0.98"}}),"\"ok\":1"),"typed voltage gain accepted");
  settle(1); check(V_GAIN==0.98f,"typed voltage gain applied");
  check(has(call("/pin",HTTP_POST,{{"pin","1234"},{"new","12a4"}}),"4 digits"),"PIN with a letter refused");
  check(has(call("/pin",HTTP_POST,{{"pin","1234"},{"new","4321"}}),"\"ok\":1"),"PIN changed to 4321");
  check(has(call("/unlock",HTTP_POST,{{"pin","1234"}}),"Wrong PIN"),"old PIN no longer works");
  check(has(call("/unlock",HTTP_POST,{{"pin","4321"}}),"\"ok\":1"),"new PIN works");
  printf("--- reboot keeps calibration and PIN ---\n");
  setup();
  check(V_GAIN==0.98f&&!strcmp(pinCode,"4321"),"V_GAIN 0.98 and PIN 4321 loaded from flash");
  printf("--- PIN lockout ---\n");
  for(int k=0;k<4;k++) call("/unlock",HTTP_POST,{{"pin","1111"}});
  check(has(call("/unlock",HTTP_POST,{{"pin","1111"}}),"Wait a minute"),"5th wrong PIN locks");
  check(has(call("/unlock",HTTP_POST,{{"pin","4321"}}),"Wait a minute"),"right PIN refused while locked");
  fakeT+=61000000ULL;
  check(has(call("/unlock",HTTP_POST,{{"pin","4321"}}),"\"ok\":1"),"right PIN works after 61 s");
  printf("--- zero over HTTP ---\n");
  load(0,1,12.2);
  check(has(call("/zero",HTTP_POST,{{"pin","4321"}}),"\"ok\":1"),"zero request accepted");
  handleRequests(); check(zeroState==2,"zeroing finished"); check(has(call("/cal",HTTP_GET),"\"zero\":2"),"GET /cal reports it");
  check(call("/reset",HTTP_POST)=="NO ROUTE","the energy reset route is gone");

  printf("\n%s\n",fails?"SOME CHECKS FAILED":"all checks passed");
  return fails?1:0;
}
