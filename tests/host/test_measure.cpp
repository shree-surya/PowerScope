#include <Arduino.h>
uint32_t fakeT=1000; double VPK_MV, IPK_MV, PHI, NOISE_SD, SPIKE_P;
#include "../../firmware/PowerScope/PowerScope.ino"
static void load(double amps,double pf,double sd){
  VPK_MV = 230/0.946*sqrt(2.0);
  IPK_MV = amps*sqrt(2.0)*(2.0/3.0*100.0);
  PHI = acos(pf); NOISE_SD=sd; SPIKE_P=0.004;
}
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
  return 0;
}
