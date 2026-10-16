#include "../../include/delta/instruments.h"
#include "../../include/delta/rf_graph.h"
#include <string>
#include <time.h>

// This adapter hosts the production keyboard template extracted by system_preview.py.
class String {
    std::string value;
public:
    String(const char*s=""):value(s?s:""){}
    String(const std::string&s):value(s){}
    String(int n):value(std::to_string(n)){}
    String(size_t n):value(std::to_string(n)){}
    size_t length()const{return value.length();}
    const char*c_str()const{return value.c_str();}
    void remove(size_t start,size_t length){value.erase(start,length);}
    friend String operator+(const String&a,const String&b){return String(a.value+b.value);}
};
static uint16_t*canvas;
static delta::Appearance appearance;
static delta::Status previewStatus;
static uint16_t*deltaUiCanvas(bool clear=true,bool decoration=true){if(clear)delta::ui::workBackground(canvas,appearance,decoration);return canvas;}
static const delta::Appearance&deltaUiAppearance(){return appearance;}
static const delta::Status&deltaUiStatus(){return previewStatus;}
static void deltaUiPresent(int=0,int=0,int=320,int=170){}
static String repeatString(size_t n,const char*s){std::string result;for(size_t i=0;i<n;++i)result+=s;return String(result);}
#include "delta_keyboard_preview.inc"

#if defined(_WIN32)
#define EXPORT __declspec(dllexport)
#else
#define EXPORT
#endif
extern "C" EXPORT void delta_system_frame(uint16_t*p,int theme,int scene) {
    using namespace delta;using namespace delta::ui;
    if(!p)return;canvas=p;appearance=Appearance();appearance.theme=Theme(theme%3);appearance.reducedMotion=true;
    const Appearance&a=appearance;Palette c=palette(a.theme);Status status;status.time="--:--";status.battery=78;previewStatus=status;
    MenuRow settings[]={
        {"Theme","Dark Sakura"},{"Quality","Normal"},{"Petals","Medium"},
        {"Transition","160 ms"},{"Reduced motion","",true,false,Toggle|Checked},
        {"Status icons","",true,false,Toggle|Checked},{"Rare glitch","",true,false,Toggle},{"Back"}
    };
    MenuRow files[]={ {"PREVIEW_LOGS","DIR",true,false,Folder},{"PREVIEW_SCRIPTS","DIR",true,false,Folder},
        {"PREVIEW_VERY_LONG_FILENAME_capture_2026-10-15.bin","FILE"},{"PREVIEW_README.txt","FILE"},{"Back","ACTION"} };
    RadioRow wifi[]={ {"PREVIEW_AP_A","PREVIEW WPA2  02:00:00:00:00:01",-62,6},
        {"PREVIEW_AP_B","PREVIEW Open  02:00:00:00:00:02",-88,11},
        {"PREVIEW_AP_C","PREVIEW WPA3  02:00:00:00:00:03",-51,6},{"Back","",Unmeasured,0} };
    RadioRow ble[]={ {"PREVIEW_BLE_A","PREVIEW 02:00:00:00:01:01",-73,0},
        {"PREVIEW_BLE_B","PREVIEW 02:00:00:00:01:02",-48,0},
        {"PREVIEW_BLE_C","PREVIEW 02:00:00:00:01:03",-91,0} };
    switch(scene){
    case 0:list(p,a,"Delta Appearance",settings,8,4,&status);break;
    case 1:list(p,a,"SD /PREVIEW_FILES/",files,5,2);break;
    case 2:workBackground(p,a);header(p,"SD /PREVIEW_EMPTY/",a);textClip(p,"Empty folder",21,75,277,c.muted);break;
    case 3:drawPreviewConfirmation("PREVIEW: Keep settings and return to the previous screen?","Apply",nullptr,"Back",1);break;
    case 4:progress(p,a,"PREVIEW progress","PREVIEW_COPY.bin\nSource and destination are simulated.",63,100);break;
    case 5:drawDeltaKeyboard<4,12>("PREVIEW_SECRET",63,"PREVIEW Password",qwerty_keyset,true,false,false,5,2,5);break;
    case 6:radioResults(p,a,"PREVIEW Wi-Fi scan",wifi,4,0,1,&status);break;
    case 7:radioResults(p,a,"PREVIEW BLE scan",ble,3,1,2,&status);break;
    case 8:rfidCard(p,a,"PREVIEW RFID / NFC","PREVIEW MIFARE Classic","DE AD BE EF","08","00 04",&status,"PREVIEW Pages read: 16/16");break;
    case 9:{
        workBackground(p,a,false);textClip(p,"PREVIEW RF / PULSE TIMING",9,2,301,c.text);box(p,9,18,302,1,c.line);
        line(p,0,105,319,105,c.line);for(int x=80;x<320;x+=80)line(p,x,20,x,169,c.line);
        const int pulses[]={165,280,915,270,275,800,270,280,915,260,260,810,900,270,280,300,930,280,275,810,260,265,920,280};
        for(unsigned i=0;i<sizeof(pulses)/sizeof(pulses[0]);++i){int x=int(i)*319/23,h=pulses[i]*85/1500;line(p,x,105-h/2,x,105+h/2,c.accent);}break;
    }
    case 10:{
        workBackground(p,a,false);textClip(p,"PREVIEW RF / SQUARE WAVE 433.92",9,2,301,c.text);box(p,9,18,302,1,c.line);
        const int durations[]={12,22,18,11,33,16,23,30,11,16,34,12,28,10,16,23};
        int x=10,y=54;for(int i=0;i<16;i+=2){if(x+durations[i]+durations[i+1]>310){x=10;y+=30;}line(p,x,y,x,y+6,c.accent);line(p,x,y,x+durations[i],y,c.accent);x+=durations[i];line(p,x,y,x,y+6,c.accent);line(p,x,y+6,x+durations[i+1],y+6,c.accent);x+=durations[i+1];}break;
    }
    case 11:{
        workBackground(p,a,false);header(p,"PREVIEW Terminal",a);
        const char*lines[]={"PREVIEW HOST SIMULATION","> status","display: ready","radio: not started","files: fixture only","> _"};
        for(int i=0;i<6;++i)textClip(p,lines[i],14,39+i*19,292,i==0?c.accent:c.text);break;
    }
    case 12:{MenuRow rows[]={ {"PREVIEW a very long submenu label that must remain clipped","12345678901234567890"},
        {"Unavailable action","Not ready",false},{"Checked choice","",true,true},{"Unknown UTF-8: \xe6\xa1\x9c\xf0\x9f\x8c\xb8"},{"Back"} };
        list(p,a,"PREVIEW long title with clipping and fallback",rows,5,0,&status);break;
    }
    case 13:radioResults(p,a,"PREVIEW empty scan",nullptr,0,0,1,&status);break;
    case 14:drawPreviewStatus("PREVIEW: Storage could not be mounted. Existing data is preserved. No automatic format is performed.",0xf800);break;
    case 15:drawDeltaKeyboard<4,4>("DEADBEEF",20,"PREVIEW UID (hex)",hex_keyset,false,false,false,2,1,5);break;
    case 16:drawDeltaKeyboard<3,4>("433.92",16,"PREVIEW frequency",numpad_keyset,false,false,false,2,1,5);break;
    case 17:drawDeltaKeyboard<4,12>("PREVIEW_LONG_VALUE_THAT_SCROLLS_TO_CARET",63,"PREVIEW shifted keyboard",qwerty_keyset,false,true,false,1,-1,5);break;
    case 18:{
        const int16_t samples[]={-93,-94,-92,-91,-88,-89,-91,-85,-76,-65,-58,-50,-46,-49,-55,-61,-70,-78,-84,-89,-91,-93,-94,-91};
        rfGraph(p,a,samples,nullptr,24,433.92f);break;
    }
    case 19:{
        const int16_t samples[]={-94,-92,-87,-81,-69,-55,-48,-61,-77,-89,-92,-94};
        const float frequencies[]={433.05f,433.15f,433.25f,433.35f,433.45f,433.55f,433.65f,433.75f,433.85f,433.95f,434.05f,434.15f};
        rfGraph(p,a,samples,frequencies,12);break;
    }
    default:list(p,a,"PREVIEW Empty",nullptr,0,0,&status);break;
    }
}
extern "C" EXPORT double delta_system_benchmark(int scene,int iterations) {
    uint16_t pixels[delta::Width*delta::Height];clock_t begin=clock();volatile uint16_t sample=0;
    for(int i=0;i<iterations;++i){delta_system_frame(pixels,0,scene);sample^=pixels[i%(delta::Width*delta::Height)];}
    return double(clock()-begin)*1000000.0/CLOCKS_PER_SEC/iterations+sample*0.0;
}
