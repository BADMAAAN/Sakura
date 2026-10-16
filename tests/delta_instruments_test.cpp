#include "../include/delta/instruments.h"
#include <assert.h>
#include <stdio.h>
static uint16_t guarded[delta::Width*delta::Height+2], copy[delta::Width*delta::Height];
static uint16_t*p=guarded+1;
static void intact(){assert(guarded[0]==0x1234);assert(guarded[delta::Width*delta::Height+1]==0xabcd);}
static unsigned digest(){unsigned h=2166136261U;for(int i=0;i<delta::Width*delta::Height;++i)h=(h^p[i])*16777619U;return h;}
int main(){
    using namespace delta;using namespace delta::ui;
    guarded[0]=0x1234;guarded[Width*Height+1]=0xabcd;
    Appearance a;Status s;s.time="12:34";
    RadioRow wifi[]={ {"Measured AP","WPA2  AA:BB:CC:DD:EE:FF",-62,6},{"Other AP","Open  11:22:33:44:55:66",-88,11},{"Hidden","WPA3",-51,6},{"Back","",Unmeasured,0} };
    RadioRow assignment;assignment={"Brace assignment","Metadata",-73,1};assert(assignment.metric==-73);
    assert(signalBars(Unmeasured)==0);assert(signalBars(-97)==0);assert(signalBars(-94)==1);assert(signalBars(-71)==2);assert(signalBars(-61)==3);assert(signalBars(-51)==4);assert(signalBars(-50)==5);
    for(int t=0;t<3;++t){a.theme=Theme(t);
        radioResults(p,a,"Wi-Fi scan",wifi,4,0,1,&s);intact();unsigned original=digest();
        radioResults(p,a,"Wi-Fi scan",wifi,4,0,1,&s);assert(digest()==original); // No invented time history.
        wifi[0].metric=-42;radioResults(p,a,"Wi-Fi scan",wifi,4,0,1,&s);assert(digest()!=original);wifi[0].metric=-62;intact();
        radioResults(p,a,"Wi-Fi scan",wifi,4,999,1,&s);intact();
        radioResults(p,a,"BLE scan",wifi,4,-30,2,&s);intact();
        radioResults(p,a,"Empty",nullptr,0,0,1);intact();
        rfidCard(p,a,"RFID / NFC","MIFARE Classic","01 23 45 67","08","00 04",&s,"Pages read: 16/16");intact();
        rfidCard(p,a,"RFID / NFC",nullptr,nullptr,nullptr,nullptr);intact();
        assert(rfHeat(a.theme,-200)==rfHeat(a.theme,-100));assert(rfHeat(a.theme,2147483647)==rfHeat(a.theme,-30));
        assert(rfHeat(a.theme,-95)!=rfHeat(a.theme,-40));
    }
    radioResults(nullptr,a,"Null",wifi,4,0,1);rfidCard(nullptr,a,"Null",nullptr,nullptr,nullptr,nullptr);
    puts("DELTA instruments: measured snapshots, raw RSSI, clipping, empty/null data, palettes and card fields PASS");
}
