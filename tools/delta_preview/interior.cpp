#include <delta/instruments.h>
#include <delta/rf_graph.h>
#ifndef DELTA_INTERIOR_BASELINE
#include <delta/section_ui.h>
#include <delta/ble_surface.h>
#elif defined(DELTA_BLE_BEFORE_AVAILABLE)
#include "ble_before.h"
#endif
#include <chrono>
#include <stdio.h>
#include <stdlib.h>

// Explicit host fixtures only. Never invokes radio, storage or firmware handlers.
static bool render(uint16_t *p, int theme, int scene) {
    using namespace delta; using namespace delta::ui;
    Appearance a; a.theme = Theme(theme % 3); a.reducedMotion = true;
    Status status; status.time = "--:--"; status.battery = -1;
    MenuRow wifi[] = {{"Connect to Wifi"},{"Start WiFi AP"},{"Wifi Atks"},{"Evil Portal"},{"NetCut"},
        {"Listen TCP"},{"Client TCP"},{"SOCKS4 Proxy"},{"TelNET"},{"SSH"},{"Sniffer"},{"Channel Analyzer"},
        {"Jam Detect"},{"Scan Hosts"},{"Wireguard"},{"Responder"},{"Fluxgotchi"},{"WiFi Pass Recovery"},{"Config"},{"Main Menu"}};
    RadioRow networks[] = {{"FIXTURE_AP_A","WPA2/PSK  02:00:00:00:00:01",-62,6},
        {"FIXTURE_AP_B","Open  02:00:00:00:00:02",-88,11},
        {"FIXTURE_AP_C","WPA3  02:00:00:00:00:03",-51,6},{"Back","",Unmeasured,0}};
    const char *ble[] = {"Apple Pairing Prompt","Apple Action Modal","Apple Not Your Device","Android Device Alert"};
    const char *values[] = {"30 ms","100 ms","MAX","Every Packet"};
    const char *themes[] = {"Dark Sakura","Sakura Dawn","Monochrome"};
    MenuRow settings[] = {{"Theme",themes[theme % 3]},{"Quality","Normal"},{"Petals","Normal"},{"Transition","160 ms"},
        {"Subtle Glitch","",true,false,Toggle},{"Reduced Motion","",true,false,Toggle},
        {"Status","",true,false,Toggle|Checked},{"Icons","",true,false,Toggle|Checked},
        {"Brightness"},{"Favorites"},{"Ambient Clock"},{"Back"}};
    const int16_t samples[] = {-94,-92,-87,-81,-69,-55,-48,-61,-77,-89,-92,-94};
    const float bins[] = {433.05f,433.15f,433.25f,433.35f,433.45f,433.55f,433.65f,433.75f,433.85f,433.95f,434.05f,434.15f};
    switch (scene) {
    case 0:
#ifdef DELTA_INTERIOR_BASELINE
        list(p,a,"WiFi",wifi,20,0,&status);
#else
        wifiMain(p,a,wifi,20,0,&status);
#endif
        break;
    case 1: radioResults(p,a,"Wi-Fi networks",networks,4,0,1,&status); break;
    case 2:
#if defined(DELTA_INTERIOR_BASELINE) && defined(DELTA_BLE_BEFORE_AVAILABLE)
        deltaBleBefore(p,0);
#elif defined(DELTA_INTERIOR_BASELINE)
        return false;
#else
        bleList(p,a,"BLE Spam",ble,4,0,8,1,&status);
#endif
        break;
    case 3:
#if defined(DELTA_INTERIOR_BASELINE) && defined(DELTA_BLE_BEFORE_AVAILABLE)
        deltaBleBefore(p,1);
#elif defined(DELTA_INTERIOR_BASELINE)
        return false;
#else
        bleConfig(p,a,"Apple Action Modal",values,1,-1,&status);
#endif
        break;
    case 4: rfGraph(p,a,samples,bins,12); break;
    case 5: list(p,a,"Delta Appearance",settings,12,2,&status); break;
    case 6: radioResults(p,a,"Wi-Fi networks",networks,4,3,1,&status); break;
    case 7: radioResults(p,a,"Wi-Fi networks",nullptr,0,0,1,&status); break;
    case 8: rfGraph(p,a,nullptr,nullptr,0,433.92f); break;
    case 9: {
        RadioRow international[] = {{u8"Сакура / Réseau / 桜","WPA2/PSK  02:00:00:00:00:04",-72,13},
            {"FIXTURE_very_long_network_name_123456","",Unmeasured,0},
            {u8"شبكة / רשת","Open",-90,1},{"Back","",Unmeasured,0}};
        radioResults(p,a,"Wi-Fi networks",international,4,0,1,&status); break;
    }
    default: return false;
    }
    return true;
}

int main(int argc,char **argv) {
    if(argc<4)return 1;
    uint16_t pixels[delta::Width*delta::Height];
    const int theme=atoi(argv[1]),scene=atoi(argv[2]);
    if(!render(pixels,theme,scene))return 2;
    if(argv[3][0]=='-'&&argv[3][1]=='b') {
        const int iterations=argc>4?atoi(argv[4]):100;
        if(iterations<=0)return 1;
        const auto begin=std::chrono::steady_clock::now();volatile uint16_t check=0;
        for(int i=0;i<iterations;++i){render(pixels,theme,scene);check^=pixels[i%(delta::Width*delta::Height)];}
        const double elapsed=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count();
        printf("%.3f\n",elapsed/iterations+check*0.0);return 0;
    }
    FILE *file=fopen(argv[3],"wb");if(!file)return 3;
    fprintf(file,"P6\n320 170\n255\n");
    for(int i=0;i<delta::Width*delta::Height;++i) {
        const uint16_t v=pixels[i];
        const unsigned char rgb[]={static_cast<unsigned char>((v>>11)*255/31),static_cast<unsigned char>(((v>>5)&63)*255/63),static_cast<unsigned char>((v&31)*255/31)};
        fwrite(rgb,1,3,file);
    }
    fclose(file);return 0;
}
