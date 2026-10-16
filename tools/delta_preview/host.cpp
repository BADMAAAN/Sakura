#include "../../include/delta/core.h"
#include <time.h>

static const char *labels[] = {"Wi-Fi","Bluetooth","RF","LoRa","Infrared","Ethernet","RFID / NFC","Files","Scripts","Clock","Tools","Settings","Favorites"};
extern "C" {
__declspec(dllexport) void delta_frame(uint16_t *pixels,int theme,int petals,int quality,int reduced,int glitch,
    int icons,int status,int selected,int previous,uint32_t changed,int direction,uint32_t now,int ambient,int speed) {
    delta::Appearance a;a.theme=delta::Theme(theme%3);a.petals=uint8_t(petals);a.quality=uint8_t(quality);
    a.reducedMotion=reduced;a.glitch=glitch;a.icons=icons;a.status=status;
    a.transitionMs=uint16_t(speed);
    delta::Navigation n;n.selected=selected%13;n.previous=previous%13;n.changedAt=changed;n.direction=direction;
    delta::Status s;s.time="21:04";s.battery=78;s.wifi=true;
    delta::render(pixels,a,n,delta::Icon(n.selected),labels[n.selected],delta::Icon(n.previous),labels[n.previous],s,now,ambient);
}
__declspec(dllexport) double delta_benchmark(int iterations) {
    uint16_t pixels[delta::Width*delta::Height];
    clock_t begin=clock();
    volatile uint16_t sample=0;
    for(int i=0;i<iterations;++i){delta_frame(pixels,0,3,2,0,1,1,1,i%13,(i+12)%13,i*17-30,1,i*17,0,160);sample^=pixels[i%54400];}
    return double(clock()-begin)*1000000.0/CLOCKS_PER_SEC/iterations+sample*0.0;
}
}
