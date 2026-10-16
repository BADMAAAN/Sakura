#include "../include/delta/instruments.h"
#include "../include/delta/section_ui.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint16_t guarded[delta::Width*delta::Height+2];
static uint16_t*const frame=guarded+1;
static void bounds(){assert(guarded[0]==0x1324);assert(guarded[delta::Width*delta::Height+1]==0xabcd);}
static unsigned digest(){unsigned h=2166136261u;for(int i=0;i<delta::Width*delta::Height;++i)h=(h^frame[i])*16777619u;return h;}
int main() {
    using namespace delta;using namespace delta::ui;
    guarded[0]=0x1324;guarded[Width*Height+1]=0xabcd;
    Status status;status.time="--:--";status.battery=-1;
    Appearance a;
    MenuRow menu[]={{"Connect to WiFi"},{"Start WiFi AP"},{"An intentionally long tool name which must fit the viewport"},{"Config"},{"Back"}};
    RadioRow rows[]={{"Measured network","WPA2/PSK  01:23:45:67:89:AB",-63,6},{"Другой SSID / long network name","Open  12:34:56:78:90:AB",-92,11},{"Unknown RSSI","WPA3  12:34:56:78:90:AC",Unmeasured,6},{"Back","",Unmeasured,0}};
    assert(!radioObserved(rows[3]));assert(radioObserved(rows[2]));
    int bins[14],maximum;
    radioHistogram(rows,4,1,bins,maximum);assert(maximum==2&&bins[5]==2&&bins[10]==1);
    int sum=0;for(int i=0;i<14;++i)sum+=bins[i];assert(sum==3);
    radioHistogram(rows,4,2,bins,maximum);sum=0;for(int i=0;i<14;++i)sum+=bins[i];assert(sum==2);
    radioHistogram(nullptr,0,1,bins,maximum);assert(maximum==0);
    char security[32];radioSecurity(rows[0].detail,security,sizeof(security));assert(!strcmp(security,"WPA2/PSK"));
    radioSecurity(nullptr,security,sizeof(security));assert(!security[0]);radioSecurity("Long security",security,1);assert(!security[0]);
    char small[5];radioSecurity("WPA2/WPA3/PSK  MAC",small,5);assert(!strcmp(small,"WPA2"));
    assert(typeface::lineHeight(typeface::Face::Body)==18&&typeface::lineHeight(typeface::Face::Numeric)==30);
    for(int theme=0;theme<3;++theme) {
        a.theme=Theme(theme);
        for(int index=-1;index<7;++index){wifiMain(frame,a,menu,5,index,&status);bounds();radioResults(frame,a,"Wi-Fi scan",rows,4,index,1,&status);bounds();}
        wifiMain(frame,a,menu,5,1,&status,"Disconnected",200);bounds();
        wifiMain(frame,a,nullptr,0,100,&status);bounds();wifiMain(nullptr,a,menu,5,0);
        radioResults(frame,a,"Wi-Fi scan",rows,4,0,1,&status);const unsigned before=digest();
        radioResults(frame,a,"Wi-Fi scan",rows,4,0,1,&status);assert(before==digest());
        rows[0].metric=-45;radioResults(frame,a,"Wi-Fi scan",rows,4,0,1,&status);assert(before!=digest());rows[0].metric=-63;
        radioResults(frame,a,"BLE scan",rows,4,0,2,&status,-20);bounds();
        radioResults(frame,a,"Wi-Fi scan",nullptr,0,0,1);bounds();radioResults(nullptr,a,"No frame",rows,4,0,1);
    }
    puts("Wi-Fi interior: real snapshot histogram, action/unknown distinction, all themes, long UTF-8 labels and framebuffer bounds PASS");
}
