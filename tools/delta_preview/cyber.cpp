#include <delta/section_ui.h>
#include <delta/ble_surface.h>
#include <delta/rf_graph.h>
#include <chrono>
#include <cstdio>
#include <cstdlib>

// Layout fixtures only. These values never enter firmware or invoke radio handlers.
static void menu(uint16_t*p,const delta::Appearance&a,const char*oldTitle,const char*title,
                 const char*subtitle,const delta::ui::MenuRow*rows,int count,int selected,const delta::Status&s) {
#ifdef DELTA_CYBER_BASELINE
    delta::ui::list(p,a,oldTitle,rows,count,selected,&s);
#else
    delta::ui::modeList(p,a,title,subtitle,rows,count,selected,&s);
#endif
}
static bool render(uint16_t*p,int theme,int scene) {
    using namespace delta;using namespace delta::ui;
    Appearance a;a.theme=Theme(theme%3);a.reducedMotion=true;
    Status status;status.time="--:--";status.battery=-1;
    MenuRow wifi[]={{"Connect to Wifi"},{"Start WiFi AP"},{"Wifi Atks"},{"Evil Portal"},{"NetCut"},
        {"Listen TCP"},{"Client TCP"},{"SOCKS4 Proxy"},{"TelNET"},{"SSH"},{"Sniffer"},{"Channel Analyzer"},
        {"Jam Detect"},{"Scan Hosts"},{"Wireguard"},{"Responder"},{"Fluxgotchi"},{"WiFi Pass Recovery"},{"Config"},{"Main Menu"}};
    RadioRow networks[]={{"FIXTURE_AP_A","WPA2/PSK  02:00:00:00:00:01",-62,6},
        {"FIXTURE_AP_B","Open  02:00:00:00:00:02",-88,11},
        {"FIXTURE_AP_C","WPA3  02:00:00:00:00:03",-51,6},{"Back","",Unmeasured,0}};
    MenuRow bleMain[]={{"Media Cmds"},{"BLE Scan"},{"iBeacon"},{"Bad BLE"},{"BLE Keyboard"},{"BLE Spam"},
        {"BLE Suite"},{"Ninebot"},{"Presenter mode"},{"Main Menu"}};
    const char*bleModes[]={"Apple Pairing Prompt","Apple Action Modal","Apple Not Your Device","Android Device Alert"};
    const char*values[]={"30 ms","100 ms","MAX","Every Packet"};
    MenuRow rfMain[]={{"Scan/copy"},{"Record RAW"},{"Custom SubGhz"},{"Spectrum"},{"RSSI Spectrum"},
        {"SquareWave Spec"},{"Spectogram"},{"Bruteforce"},{"Jammer"},{"Config"},{"Main Menu"}};
    const int16_t samples[]={-94,-92,-87,-81,-69,-55,-48,-61,-77,-89,-92,-94};
    const float bins[]={433.05f,433.15f,433.25f,433.35f,433.45f,433.55f,433.65f,433.75f,433.85f,433.95f,434.05f,434.15f};
    MenuRow settings[]={{"Delta Appearance"},{"Display & UI"},{"Customization"},{"Audio Config"},{"System Config"},{"Power"},{"About"},{"Main Menu"}};
    const char*themes[]={"Dark Sakura","Sakura Dawn","Monochrome"};
    MenuRow appearance[]={{"Theme",themes[theme%3]},{"Quality","Normal"},{"Petals","Normal"},{"Transition","160 ms"},
        {"Subtle Glitch","",true,false,Toggle},{"Reduced Motion","",true,false,Toggle},
        {"Status","",true,false,Toggle|Checked},{"Icons","",true,false,Toggle|Checked},{"Brightness"},{"Favorites"},{"Ambient Clock"},{"Back"}};
    MenuRow files[]={{"SD Card","",true,false,Folder},{"LittleFS","",true,false,Folder},{"WebUI"},{"Connect"},{"Mass Storage"},{"Main Menu"}};
    MenuRow scripts[]={{"Load..."},{"Main Menu"}};
    switch(scene) {
    case 0: {
        Navigation nav;nav.selected=0;nav.previous=0;a.reducedMotion=false;
        delta::render(p,a,nav,Icon::Wifi,"WiFi",Icon::Wifi,"WiFi",status,4260);break;
    }
    case 1:wifiMain(p,a,wifi,20,0,&status);break;
    case 2:radioResults(p,a,"Wi-Fi networks",networks,4,0,1,&status);break;
    case 3:menu(p,a,"Bluetooth","BLUETOOTH","BLUETOOTH TOOLS",bleMain,10,5,status);break;
    case 4:bleList(p,a,"BLE Spam",bleModes,4,0,8,1,&status);break;
    case 5:bleConfig(p,a,"Apple Action Modal",values,1,-1,&status);break;
    case 6:menu(p,a,"Radio Frequency (CC1101)","RF CONTROL","Radio Frequency (CC1101)",rfMain,11,4,status);break;
    case 7:rfGraph(p,a,samples,bins,12);break;
    case 8:menu(p,a,"Config","SETTINGS","DEVICE CONFIGURATION",settings,8,0,status);break;
    case 9:menu(p,a,"Delta Appearance","APPEARANCE","DELTA / VISUAL",appearance,12,2,status);break;
    case 10:menu(p,a,"Files","FILES","STORAGE",files,6,0,status);break;
    case 11:menu(p,a,"Scripts","SCRIPTS","SCRIPT TOOLS",scripts,2,0,status);break;
    case 12:radioResults(p,a,"Wi-Fi networks",nullptr,0,0,1,&status);break;
    case 13:networks[0].metric=Unmeasured;radioResults(p,a,"Wi-Fi networks",networks,4,0,1,&status);break;
    case 14:rfGraph(p,a,nullptr,nullptr,0,433.92f);break;
    default:return false;
    }
    return true;
}
int main(int argc,char**argv) {
    if(argc<4)return 1;uint16_t pixels[delta::Width*delta::Height];
    const int theme=atoi(argv[1]),scene=atoi(argv[2]);if(!render(pixels,theme,scene))return 2;
    if(argv[3][0]=='-'&&argv[3][1]=='b') {
        int iterations=argc>4?atoi(argv[4]):100;if(iterations<1)return 1;
        const auto start=std::chrono::steady_clock::now();volatile uint16_t check=0;
        for(int i=0;i<iterations;++i){render(pixels,theme,scene);check^=pixels[i%(delta::Width*delta::Height)];}
        double elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-start).count();
        printf("%.3f\n",elapsed/iterations+check*0.0);return 0;
    }
    FILE*file=fopen(argv[3],"wb");if(!file)return 3;fprintf(file,"P6\n320 170\n255\n");
    for(int i=0;i<delta::Width*delta::Height;++i) {
        uint16_t v=pixels[i];unsigned char rgb[]={static_cast<unsigned char>((v>>11)*255/31),
            static_cast<unsigned char>(((v>>5)&63)*255/63),static_cast<unsigned char>((v&31)*255/31)};
        fwrite(rgb,1,3,file);
    }
    fclose(file);return 0;
}
