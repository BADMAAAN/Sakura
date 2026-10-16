#include <cassert>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <functional>
#include <delta/button_input.h>
#include "core/input_diagnostics.h"
struct String:std::string{
 using std::string::string;
 String(const std::string&s):std::string(s){}
 String(int n):std::string(std::to_string(n)){}
 String(size_t n):std::string(std::to_string(n)){}
 String(char c):std::string(1,c){}
 void remove(size_t n){erase(n);}
 String substring(size_t a,size_t b=std::string::npos)const{return substr(a,b==std::string::npos?b:b-a);}
};
#define SAKURA_BUILD 1
#define T_EMBED_1101 1
#define FLUX_DELTA_BUILD 1
#define HAS_ENCODER 1
#define HAS_1_BUTTON 0
#define BTN_ACT 0
#define SEL_BTN 0
#define BK_BTN 6
#define LW 6
#define LH 8
#define FM 2
#define FP 1
#define TFT_DARKGREY 0
#define portTICK_PERIOD_MS 1
volatile bool EscPress=false,SelPress=false,NextPress=false,PrevPress=false,AnyKeyPress=false,SerialCmdPress=false;
volatile bool UpPress=false,DownPress=false,NextPagePress=false,PrevPagePress=false;
bool LongPress=false,DiscardEncoderInput=false,isSleeping=false,screenOff=false;
int32_t RotaryNetSteps=0;
bool sakuraKeyboardActive=false,deltaKeyboardActive=false;
bool keyboardInputActive(){return sakuraKeyboardActive;}
int tftWidth=320,tftHeight=170,max_FM_size=25,max_FP_size=50;
uint32_t clockMs=0;uint32_t millis(){return clockMs;}uint32_t micros(){return clockMs*1000;}
int selectPin=1,backPin=1;
int digitalRead(int pin){return pin==SEL_BTN?selectPin:backPin;}
bool wakeUpScreen(){bool old=screenOff;screenOff=false;return old;}
struct{void Clear(){}bool pressed=false;}touchPoint;
struct Encoder{int position=0;int getPosition(){return position;}}fakeEncoder,*encoder=&fakeEncoder;
void inputDiagnosticEvent(InputSource,InputKind,uint32_t){}
void inputDiagnosticConsumed(bool){}
void deltaUiPhysicalInput(uint32_t){}
/* CHECK */
/* CLEAR */
int32_t drainRotarySteps(){int32_t n=RotaryNetSteps;RotaryNetSteps=0;return n;}
void InputHandler();
std::vector<std::function<void()>> actions;
size_t actionIndex=0;uint32_t nextAction=40;
int focusX=0,focusY=-1;bool focusCaps=false;String visible;
int powerCalls=0;bool sleepOnHold=false;uint32_t heldSince=0;
uint32_t selectRawAt=0,selectEdgeAt=0,selectActionAt=0;
void checkReboot(){++powerCalls;if(sleepOnHold&&clockMs-heldSince>=800)isSleeping=true;}
void vTaskDelay(int n){
 clockMs+=n;assert(clockMs<200000 && "controller failed to exit");
 if(clockMs>=nextAction&&actionIndex<actions.size()){
  nextAction=clockMs+40;actions[actionIndex++]();
 }
 InputHandler();
 if(SelPress&&selectEdgeAt==0)selectEdgeAt=clockMs;
}
void yield(){}void delay(int n){clockMs+=n;}
int credentialSaves=0,networkStarts=0;
struct{int bgColor=0,priColor=1;String getWifiPassword(const String&){return "";}void addWifiCredential(const String&,const String&){++credentialSaves;}}fluxConfig;
struct Display{
 template<class...T>void setCursor(T...){}
 template<class...T>void setTextColor(T...){}
 template<class...T>void setTextSize(T...){}
 template<class...T>void fillScreen(T...){}
 template<class...T>void fillRect(T...){}
 template<class...T>void drawRect(T...){}
 template<class...T>void drawString(T...){}
 template<class...T>void print(T...){}
 int getCursorX(){return 0;}int getCursorY(){return 0;}
}tft;
void resetTftDisplay(){}int getComplementaryColor2(int n){return n;}
String repeatString(size_t n,const char*s){String out;while(n--)out+=s;return out;}
uint16_t canvas;uint16_t*deltaUiCanvas(bool){return &canvas;}
template<int H,int W>bool drawDeltaKeyboard(const String&value,int,const String&,char keys[H][W][2],bool,bool caps,bool,int x,int y,int count){
 assert(y>=-1&&y<H&&x>=0&&x<(y<0?count:W));
 if(y>=0)assert(keys[y][x][caps]!=0 && "NUL cell focused");
 focusX=x;focusY=y;focusCaps=caps;visible=value;return true;
}
enum KeyboardAction{KEYBOARD_CONTINUE,KEYBOARD_OK,KEYBOARD_CANCEL,KEYBOARD_REDRAW};
/* BOARD */
/* KEYBOARD */
enum class WifiConnectOutcome{Failed,Cancelled,Connected,Busy};
String wifiKeyboardResult,wifiIP;bool wifiConnected=false;
bool wifiBack(bool){return false;}
String keyboard(String,int,const char*,bool){return wifiKeyboardResult;}
bool _connectToWifiNetwork(const String&,const String&,WifiConnectOutcome*out){++networkStarts;*out=WifiConnectOutcome::Connected;return true;}
void stopOwnedWifiAttempt(bool){}
struct Option{const char*label;std::function<void()>action;};std::vector<Option>options;
int loopOptions(const std::vector<Option>&){assert(false);return -1;}
struct{struct IP{String toString(){return "inert";}};IP localIP(){return {};}}WiFi;
void*timezoneTaskHandle=(void*)1;void updateTimezoneTask(void*){}
template<class...T>void xTaskCreate(T...){}
/* WIFI */
void step(int n){actions.push_back([=](){encoder->position-=n;if(n<0)nextAction=clockMs+uint32_t(-n)*4+40;});actions.push_back([](){});}
void select(){actions.push_back([](){selectPin=0;selectRawAt=clockMs;selectEdgeAt=0;});actions.push_back([](){selectPin=1;});actions.push_back([](){});}
void back(){actions.push_back([](){backPin=0;});actions.push_back([](){backPin=1;});actions.push_back([](){});}
void expect(int x,int y,const String&v){actions.push_back([=](){assert(focusX==x&&focusY==y&&visible==v);});}
template<int H,int W>void run(char keys[H][W][2],const String&scenario,int holdFocus){
 std::vector<std::pair<int,int>>cells;for(int i=0;i<5;++i)cells.push_back({i,-1});
 for(int y=0;y<H;++y)for(int x=0;x<W;++x)if(keys[y][x][0])cells.push_back({x,y});
 int total=cells.size();String initial,result;
 if(scenario=="reach"){
  for(int caps=0;caps<2;++caps){
   if(caps){step(1);select();step(-1);}
   for(int i=0;i<total;++i){auto c=cells[i];expect(c.first,c.second,"");step(1);}expect(0,-1,"");
  }
  // Every row can rotate to all five commands without physical Back.
  for(int row=0;row<H;++row){int offset=5;for(int r=0;r<row;++r)for(int x=0;x<W;++x)offset+=keys[r][x][0]!=0;
   step(offset);for(int command=0;command<5;++command){step((total-offset+command)%total);expect(command,-1,"");step(-command);offset=0;}}
  select();
 }else if(scenario=="back"){
  step(5);expect(0,0,"");for(int i=0;i<5;++i){back();expect(0,0,"");}
  select();expect(0,0,String(keys[0][0][0]));step(1);select();
  String text;text+=keys[0][0][0];text+=keys[0][1][0];expect(1,0,text);step(-6);select();initial="";
 }else if(scenario=="edit"){
  initial="xy";step(2);select();expect(2,-1,"x");step(1);select();expect(3,-1,"x ");step(-2);select();
  step(4);select();String text="x ";text+=keys[0][0][1];expect(0,0,text);step(-5);select();
 }else if(scenario=="cancel"){
  initial="partial";step(4);select();
 }else if(scenario=="reopen"){
  EscPress=SelPress=true;RotaryNetSteps=99;expect(0,-1,"");step(4);select();
 }else if(scenario=="wake"){
  isSleeping=true;select();actions.push_back([](){assert(!SelPress);isSleeping=false;});expect(0,-1,"");step(4);select();
 }else if(scenario=="burst"){
  step(total*11+3);expect(3,-1,"");step(-total*13-2);expect(1,-1,"");step(3);select();
 }else if(scenario=="hold"){
  step(holdFocus);expect(cells[holdFocus].first,cells[holdFocus].second,"");
  actions.push_back([](){heldSince=clockMs;backPin=0;sleepOnHold=true;});
  for(int i=0;i<15;++i)actions.push_back([](){});
  actions.push_back([](){selectPin=0;}); // Select during power countdown must not leak.
 }else assert(false);
 result=generalKeyboard<H,W>(initial,128,"INERT TEST",keys,false);
 selectActionAt=clockMs;
 assert(!keyboardInputActive()&&!EscPress&&!SelPress&&RotaryNetSteps==0);
 if(scenario=="cancel"||scenario=="reopen"||scenario=="wake"||scenario=="burst"||scenario=="hold")assert(result=="\x1B");
 if(scenario=="reach")assert(result.empty());
 if(scenario=="back"){String text;text+=keys[0][0][0];text+=keys[0][1][0];assert(result==text);}
 if(scenario=="edit"){String text="x ";text+=keys[0][0][1];assert(result==text);}
 if(scenario=="hold")assert(isSleeping&&powerCalls>0);
 if(scenario=="reach"||scenario=="cancel"){
  assert(selectEdgeAt-selectRawAt==20);
  assert(selectActionAt-selectEdgeAt<=4);
  printf("modeled recognition=%u ms; action after edge=%u ms\n",selectEdgeAt-selectRawAt,selectActionAt-selectEdgeAt);
  wifiKeyboardResult=result;WifiConnectOutcome outcome;
  bool connected=_wifiConnect("INERT TEST",8,&outcome);
  if(scenario=="cancel")assert(!connected&&outcome==WifiConnectOutcome::Cancelled&&networkStarts==0&&credentialSaves==0);
  else assert(connected&&outcome==WifiConnectOutcome::Connected&&networkStarts==1&&credentialSaves==1);
 }
 if(scenario=="reopen"){
  selectPin=backPin=1;actions.clear();actionIndex=0;for(int i=0;i<25;++i)vTaskDelay(4);
  actions.clear();actionIndex=0;nextAction=clockMs+40;expect(0,-1,"");select();
  assert((generalKeyboard<H,W>("",128,"REOPEN",keys,false).empty()));
 }
 printf("PASS %s: modeled board debounce 20 ms; controller pump 4 ms; no physical timing claim\n",scenario.c_str());
}
int main(int argc,char**argv){assert(argc>=3);String layout=argv[1],scenario=argv[2];int focus=argc>3?atoi(argv[3]):0;
 if(layout=="qwerty")run<4,12>(qwerty_keyset,scenario,focus);
 else if(layout=="hex")run<4,4>(hex_keyset,scenario,focus);
 else run<3,4>(numpad_keyset,scenario,focus);
}
