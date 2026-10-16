#include "delta/section_ui.h"
#include "delta/ble_surface.h"
#include "delta/rf_graph.h"
#include <cassert>
#include <algorithm>
#include <vector>
#include <cstdio>

int main() {
    using namespace delta; using namespace delta::ui;
    std::vector<uint16_t> guarded(Width*Height+2,0xA55A), before(Width*Height);
    uint16_t*p=guarded.data()+1;
    const char*labels[]={"First mode","Second mode","Third mode","Fourth mode"};
    const char*values[]={"30 ms","100 ms","+21 dBm","Every packet"};
    MenuRow menu[]={{"Connect Wi-Fi"},{"Start AP"},{"Configuration"},{"Back"}};
    RadioRow networks[]={{"Measured AP","WPA2  02:00:00:00:00:01",-61,6},{"Unknown","WPA3",Unmeasured,11},{"Back","",Unmeasured,0}};
    Status status;status.time="--:--";status.battery=-1;
    for(int theme=0;theme<3;++theme) {
        Appearance a;a.theme=Theme(theme);Palette c=palette(a.theme);
        for(int selected=0;selected<4;++selected) {
            bleList(p,a,"Modes",labels,4,0,4,selected,&status);
            // A fixed underline marks the one selected mode; no row viewport or footer.
            assert(p[113*Width+144]==c.accent);
            std::copy(p,p+Width*Height,before.begin());
            const char *original=labels[(selected+1)%4];labels[(selected+1)%4]="Hidden sibling";
            bleList(p,a,"Modes",labels,4,0,4,selected,&status);
            assert(std::equal(before.begin(),before.end(),p));labels[(selected+1)%4]=original;
            wifiMain(p,a,menu,4,selected,&status);
        }
        for(int selected=0;selected<5;++selected)bleConfig(p,a,"Test device",values,selected,-1,&status);
        bleConfig(p,a,"Test device",nullptr,0,0,&status);
        bleRunning(p,a,"Test device",values,3,-1,7,2.5f,true,&status);
        blePrompt(p,a,"Confirm","Keep","Delete",1,"Pending action",&status);
        radioResults(p,a,"Scan results",networks,3,0,1,&status);
        std::copy(p,p+Width*Height,before.begin());networks[0].metric=-42;
        radioResults(p,a,"Scan results",networks,3,0,1,&status);
        assert(!std::equal(before.begin(),before.end(),p));networks[0].metric=-61;
        radioResults(p,a,"Scan results",networks,3,1,1,&status);
        radioResults(p,a,"Scan results",networks,3,2,1,&status);
        assert(radioObserved(networks[1])&&!radioObserved(networks[2]));
        int16_t samples[]={Unmeasured,Unmeasured,Unmeasured};float mhz[]={433.05f,433.20f,434.79f};
        rfGraph(p,a,samples,mhz,3);std::copy(p,p+Width*Height,before.begin());samples[1]=-63;
        rfGraph(p,a,samples,mhz,3);assert(!std::equal(before.begin(),before.end(),p));
        assert(guarded.front()==0xA55A&&guarded.back()==0xA55A);
    }
    puts("Cyber modes: selected BLE item stays visible, null values, real data changes, all themes and bounds PASS");
}
