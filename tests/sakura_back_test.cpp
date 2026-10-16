#include <cassert>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <functional>
#include <string>
#include <vector>
#include <delta/button_input.h>
#include "core/input_diagnostics.h"
using std::min; using std::max;
struct String : std::string {
 using std::string::string;
 String(const std::string&s):std::string(s){}
 String(size_t n):std::string(std::to_string(n)){}
 String(int n):std::string(std::to_string(n)){}
};
#define HAS_ENCODER 1
#define SAKURA_BUILD 1
#define T_EMBED_1101 1
#define pdMS_TO_TICKS(x) (x)
#define FP 1
#define FM 2
#define LH 8
#define BORDER_PAD_Y 28
#define TFT_YELLOW 1
#define TFT_DARKGREY 2
#define TFT_BLUE 3
#define TFT_WHITE 4
#define SMOOTH_FONT 1
#define BTN_ALIAS "Select"
#define SEL_BTN 0
#define BK_BTN 6
#define BTN_ACT 0
volatile bool EscPress=false,SelPress=false,PrevPress=false,NextPress=false,UpPress=false,DownPress=false,AnyKeyPress=false,SerialCmdPress=false;
int32_t RotaryNetSteps=0;int xHandle=0;
void inputDiagnosticConsumed(bool){}
void inputDiagnosticScreen(const char*){}
void inputDiagnosticAction(const char*,const char*){}
int diagnosticLongs=0;
void inputDiagnosticEvent(InputSource,InputKind kind,uint32_t){if(kind==InputKind::Long)++diagnosticLongs;}
bool DiscardEncoderInput=false,LongPress=false,isSleeping=false,screenOff=false;
bool keyboardInputActive(){return false;}
volatile bool NextPagePress=false,PrevPagePress=false;
struct{void Clear(){}}touchPoint;
int selectPin=1,backPin=1;
int digitalRead(int pin){return pin==SEL_BTN?selectPin:backPin;}
bool wakeUpScreen(){bool old=screenOff;screenOff=false;return old;}
struct Encoder{int position=0;int getPosition(){return position;}}fakeEncoder,*encoder=&fakeEncoder;
bool returnToMenu=false;
int ticks=0, tftWidth=320,tftHeight=170;
uint32_t clockMs=0;uint32_t millis(){return clockMs;}
bool autoSelect=true;int backAt=-1;int errors=0;bool cancellingShown=false;
/* PRODUCTION_CHECK */
void delay(int){}
void yield(){assert(++ticks<8 && "Back must close wait without Select");}
void vTaskDelay(int n){assert(++ticks<1000 && "Back was discarded");clockMs+=n;if(autoSelect)SelPress=true;if(ticks==backAt)EscPress=true;}
void displayTextLine(const char*s){if(std::string(s)=="Cancelling...")cancellingShown=true;}
void displayError(const char*,bool=false){++errors;}
struct NimBLEScan {
 bool active=false,stopWorks=true,startWorks=true;int starts=0,stops=0,clears=0,detaches=0;
 int completeAt=-1;bool backOnComplete=false;
 bool isScanning(){if(active&&ticks==completeAt){active=false;if(backOnComplete)EscPress=true;}return active;}
 bool start(uint32_t,bool,bool){++starts;active=startWorks;return startWorks;}
 bool stop(){++stops;if(stopWorks)active=false;return stopWorks;}
 void clearResults(){assert(!active && "Results cannot be released while scanner runs");++clears;}
 void setScanCallbacks(void*){++detaches;}
};
bool bleScanCancellable(NimBLEScan*,uint32_t,bool=false,bool=false);
int32_t drainRotarySteps(){return 0;}
/* PRODUCTION_CLEAR */
struct {int priColor=1,bgColor=0;}fluxConfig;
struct Display {
 template<class... T>void setTextColor(T...){}
 template<class... T>void drawCentreString(T...){}
 template<class... T>void drawString(T...){}
 template<class... T>void fillRect(T...){}
 template<class... T>void setTextSize(T...){}
}tft;
void drawMainBorder(){} void drawMainBorderWithTitle(const String&){}
enum BleSpamTxPower { BLE_SPAM_TX_MAX };
enum BleSpamMacRandMode { BLE_SPAM_MAC_EVERY_PACKET };
struct BleSpamSelection{int attack_type=0,device_index=0;};
struct BleSpamConfig{uint32_t adv_ms=5,gap_ms=5;BleSpamTxPower tx_power=BLE_SPAM_TX_MAX;BleSpamMacRandMode mac_rand_mode=BLE_SPAM_MAC_EVERY_PACKET;};
struct BleSpamEditState{bool editing=false;int edit_row=0;uint32_t adv_backup=0,gap_backup=0;BleSpamTxPower tx_backup=BLE_SPAM_TX_MAX;BleSpamMacRandMode mac_backup=BLE_SPAM_MAC_EVERY_PACKET;};
struct BleSpamListState{int cursor=0,scroll=0;bool redraw=true;};
struct BleSpamListMetrics{int visible_rows=4;};
BleSpamListMetrics bleSpamGetListMetrics(int){return {};}
void bleSpamEnsureScroll(BleSpamListState&,int,int){}
template<class... T>void bleSpamRenderList(T...){}
template<class... T>void bleSpamRenderConfigRows(T...){}
const char*bleSpamGetDeviceName(int,int){return "fake";}
String bleSpamMakeTitle(const String&s){return s;}
uint32_t bleSpamAdjustMs(uint32_t n,int d){return n+d;}
using NimBLEAddress=int;
bool fakeCompleted=false;int resultScreens=0,deinits=0;
struct FastPairDeviceInfo{String name,deviceType;};
struct FastPairExploitEngine{
 std::vector<FastPairDeviceInfo>scanForFastPairDevices(int duration,bool& completed){assert(duration==10);completed=fakeCompleted;return {};}
};
struct BLEStateManager{static void deinitBLE(bool){++deinits;}};
struct AutoCleanup{std::function<void()>fn;AutoCleanup(std::function<void()>f):fn(f){}~AutoCleanup(){if(fn)fn();}void disable(){fn=nullptr;}};
void showDeviceInfoScreen(const char*,const std::vector<String>&lines,int,int){assert(lines.size()>=2&&lines[1]=="Total: 0");++resultScreens;}
/* PRODUCTION_HANDLERS */
int main(int argc,char**argv){
 assert(argc==2);String scenario=argv[1];BleSpamSelection selection;BleSpamConfig config;bool changed=false;
 auto list=[](){return bleSpamListLoop("Modes",4,0,[](int){return "fake";},"Back");};
 if(scenario=="list"){
  for(int i=0;i<100;++i){clearInputState();ticks=0;EscPress=true;assert(list()==-1);assert(!SelPress);}
 }else if(scenario=="pending"){
  EscPress=PrevPress=SelPress=true;assert(list()==-1 && "Back cannot become confirmation");
 }else if(scenario=="config"){
  EscPress=PrevPress=SelPress=true;assert(!bleSpamConfigScreen(selection,config,changed));assert(!changed);
 }else if(scenario=="prompts"){
  EscPress=PrevPress=SelPress=true;assert(!bleSpamStoppedPrompt(selection,0));
  clearInputState();ticks=0;EscPress=PrevPress=SelPress=true;assert(bleSpamTwoOptionPrompt("Saved","Use","Delete")==-1);
 }else if(scenario=="info"){
  EscPress=true;ble_info("fake","fake","-50");assert(ticks<8);
 }else if(scenario=="scan-pre"){
  autoSelect=false;NimBLEScan scan;EscPress=true;assert(!bleScanCancellable(&scan,100));assert(scan.starts==0&&!scan.active);
 }else if(scenario=="scan-cancel"){
  autoSelect=false;NimBLEScan scan;backAt=1;assert(!bleScanCancellable(&scan,100));assert(scan.starts==1&&!scan.active&&scan.stops==1&&cancellingShown);
  clearInputState();ticks=0;backAt=-1;NimBLEScan race;race.completeAt=1;race.backOnComplete=true;
  assert(!bleScanCancellable(&race,100));assert(!race.active&&race.starts==1);
  clearInputState();ticks=0;NimBLEScan complete;complete.completeAt=1;
  SelPress=PrevPress=NextPress=true;RotaryNetSteps=3;
  assert(bleScanCancellable(&complete,100));assert(!SelPress&&!PrevPress&&!NextPress&&!EscPress&&RotaryNetSteps==0);
 }else if(scenario=="scan-timeout"){
  autoSelect=false;NimBLEScan scan;scan.stopWorks=false;scan.completeAt=40;backAt=1;
  SelPress=PrevPress=true;RotaryNetSteps=3;
  assert(!bleScanCancellable(&scan,100));assert(!scan.active&&errors>0&&ticks>=40);
  assert(!SelPress&&!PrevPress&&!EscPress&&RotaryNetSteps==0);
 }else if(scenario=="fastpair"){
  runFastPairScan(0);assert(resultScreens==0&&deinits==0);
  fakeCompleted=true;runFastPairScan(0);assert(resultScreens==1&&deinits==0);
 }else if(scenario=="scan-owned"){
  autoSelect=false;NimBLEScan scan;scan.active=true;
  assert(!bleScanCancellable(&scan,100));assert(scan.active&&scan.starts==0&&scan.stops==0&&scan.clears==0);
 }else if(scenario=="scan-repeat"){
  autoSelect=false;NimBLEScan scan;
  for(int i=0;i<100;++i){clearInputState();ticks=0;backAt=1;assert(!bleScanCancellable(&scan,100));assert(!scan.active);}
  assert(scan.starts==100&&scan.stops==100);
 }else if(scenario=="edges"){
  delta::ButtonInput back,select;
  assert(back.sample(true,0)==0);assert(back.sample(false,8)==0);assert(back.sample(true,10)==0);
  assert(back.sample(true,29)==0);assert(back.sample(true,30)==1);
  for(uint32_t t=31;t<3000;t+=10)assert(back.sample(true,t)==0);
  assert(back.sample(false,3000)==0);assert(back.sample(false,3020)==-1);assert(back.sample(false,3040)==0);
  assert(select.sample(true,3041)==0);assert(select.sample(true,3061)==1);SelPress=true;
  assert(back.sample(true,3062)==0);assert(back.sample(true,3082)==1);EscPress=PrevPress=true;RotaryNetSteps=1;
  assert(!check(SelPress));assert(check(EscPress));assert(!SelPress&&!PrevPress&&RotaryNetSteps==0);
  assert(!check(EscPress)&&!check(SelPress));
  AnyKeyPress=EscPress=SelPress=PrevPress=true;RotaryNetSteps=3;
  assert(check(AnyKeyPress));assert(!check(EscPress)&&!check(SelPress)&&!PrevPress&&RotaryNetSteps==0);
 }else if(scenario=="input-held"){
  InputHandler();backPin=0;clockMs=10;InputHandler();assert(!EscPress);
  clockMs=30;InputHandler();assert(check(EscPress)&&!SelPress);
  for(clockMs=40;clockMs<2500;clockMs+=10){InputHandler();assert(!EscPress&&!SelPress);}
  assert(diagnosticLongs==1);backPin=1;InputHandler();clockMs+=20;InputHandler();assert(!EscPress);
  backPin=0;clockMs+=10;InputHandler();clockMs+=20;InputHandler();assert(check(EscPress));
 }else if(scenario=="input-fast"){
  InputHandler();selectPin=0;clockMs=10;InputHandler();clockMs=30;InputHandler();assert(SelPress);
  backPin=0;clockMs=31;InputHandler();fakeEncoder.position=2;clockMs=51;InputHandler();
  assert(EscPress&&!SelPress&&!PrevPress&&!NextPress&&RotaryNetSteps==0);assert(check(EscPress));
  clockMs=1000;InputHandler();assert(!EscPress&&!SelPress);
 }else if(scenario=="input-wake"||scenario=="input-screen"){
  isSleeping=scenario=="input-wake";screenOff=scenario=="input-screen";
  InputHandler();selectPin=0;clockMs=10;InputHandler();clockMs=30;InputHandler();
  assert(!SelPress&&!EscPress);isSleeping=false;
  clockMs=100;InputHandler();assert(!SelPress&&!EscPress);
  selectPin=1;clockMs=110;InputHandler();clockMs=150;InputHandler();assert(!SelPress&&!EscPress);
  selectPin=0;clockMs=160;InputHandler();clockMs=180;InputHandler();assert(check(SelPress));
 }else assert(false);
}
