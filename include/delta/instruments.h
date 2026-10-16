#pragma once
#include "widgets.h"
#include "typography.h"
#include <stdio.h>
#include <string.h>

namespace delta { namespace ui {
static const int16_t Unmeasured=-32768;
struct RadioRow {
    const char*label;const char*detail;int16_t metric;uint8_t channel;
    RadioRow(const char*l="",const char*d="",int16_t m=Unmeasured,uint8_t c=0):label(l),detail(d),metric(m),channel(c){}
};
inline int signalBars(int dbm) {
    if(dbm==Unmeasured)return 0;
    return dbm>=-50?5:dbm>=-60?4:dbm>=-70?3:dbm>=-80?2:dbm>=-95?1:0;
}
inline uint16_t rfHeat(Theme theme,int dbm) {
    Palette c=palette(theme);int intensity=(bounded(dbm,-100,-30)+100)*255/70;
    uint16_t middle=theme==Theme::Monochrome?color(119,121,127):theme==Theme::SakuraDawn?color(157,113,166):color(112,77,164);
    return intensity<128?blend(c.background,middle,unsigned(intensity)*2):blend(middle,c.accent,unsigned(intensity-128)*2);
}
inline int radioFirst(int count,int selected) { return count<=3?0:bounded(selected-1,0,count-3); }
inline bool radioObserved(const RadioRow&r) {
    return r.metric!=Unmeasured||r.channel!=0||(r.detail&&r.detail[0]);
}
inline void radioSecurity(const char*detail,char*out,int capacity) {
    if(!out||capacity<=0)return;int n=0;
    if(detail)while(detail[n]&&!(detail[n]==' '&&detail[n+1]==' ')&&n<capacity-1){out[n]=detail[n];++n;}
    out[n]=0;
}
inline void radioHistogram(const RadioRow*rows,int count,uint8_t kind,int*bins,int&maximum) {
    maximum=0;for(int i=0;i<14;++i)bins[i]=0;
    if(!rows||count<1)return;
    for(int i=0;i<count;++i){int bin=-1;
        if(kind==1&&rows[i].channel>=1&&rows[i].channel<=14)bin=rows[i].channel-1;
        else if(kind==2&&rows[i].metric!=Unmeasured)bin=bounded((int(rows[i].metric)+110)/6,0,13);
        if(bin>=0){++bins[bin];if(bins[bin]>maximum)maximum=bins[bin];}
    }
}
inline void radioResults(uint16_t*p,const Appearance&a,const char*title,const RadioRow*rows,int count,int selected,uint8_t kind,const Status*status=nullptr,int focusY=-1) {
    if(!p)return;workBackground(p,a);modeHeader(p,kind==1?"Wi-Fi":"BLE SCAN",title,a,status);Palette c=palette(a.theme);
    const uint16_t edge=blend(c.line,c.accent,50);
    if(!rows||count<=0){icon(p,kind==1?Icon::Wifi:Icon::Bluetooth,141,74,c.accent);typeface::clip(p,"No scan results",96,122,210,c.text);return;}
    selected=bounded(selected,0,count-1);const int first=radioFirst(count,selected);
    box(p,201,63,1,84,edge);
    // Retain the caller's 39 + 36*row animation coordinates at the API boundary.
    const int highlight=focusY<0?62+(selected-first)*29:62+(bounded(focusY,39,111)-39)*29/36;
    box(p,11,highlight,182,28,c.focus,3);box(p,11,highlight+6,2,16,c.accent);
    for(int i=first;i<count&&i<first+3;++i){const int y=62+(i-first)*29;
        typeface::clip(p,rows[i].label,20,y,165,i==selected?c.text:c.muted);
        char value[40];
        if(!radioObserved(rows[i]))snprintf(value,sizeof(value),"Menu action");
        else if(rows[i].metric==Unmeasured)snprintf(value,sizeof(value),"RSSI unavailable");
        else if(kind==1&&rows[i].channel)snprintf(value,sizeof(value),"%d dBm   CH %u",int(rows[i].metric),unsigned(rows[i].channel));
        else snprintf(value,sizeof(value),"%d dBm",int(rows[i].metric));
        typeface::clip(p,value,20,y+15,165,c.muted,typeface::Small);
    }
    const RadioRow&item=rows[selected];const bool measured=radioObserved(item);
    typeface::clip(p,measured?"RSSI / dBm":"NAVIGATION",211,62,96,c.muted,typeface::Small);
    if(measured) {
        char metric[16];if(item.metric==Unmeasured)snprintf(metric,sizeof(metric),"--");else snprintf(metric,sizeof(metric),"%d",int(item.metric));
        typeface::clip(p,metric,209,77,96,c.text,typeface::Numeric);
        const int strength=signalBars(item.metric);
        for(int i=0;i<5;++i){const int h=3+i*2;box(p,211+i*19,118-h,12,h,i<strength?c.accent:c.line);}
        char channel[24];if(kind==1&&item.channel)snprintf(channel,sizeof(channel),"CHANNEL %u",unsigned(item.channel));
        else snprintf(channel,sizeof(channel),kind==1?"CHANNEL --":"BLE DEVICE");
        typeface::clip(p,channel,210,120,96,c.text,typeface::Small);
        char security[40];radioSecurity(item.detail,security,sizeof(security));
        typeface::clip(p,kind==1?(security[0]?security:"Security unknown"):"Advertisement",210,136,96,c.muted,typeface::Small);
    } else {
        icon(p,kind==1?Icon::Wifi:Icon::Bluetooth,240,83,c.accent);
        typeface::clip(p,"SELECT: OPEN",210,124,95,c.text,typeface::Small);
    }
    box(p,14,153,292,1,edge);
    if(kind==1) {
        const char *address = item.detail ? strstr(item.detail,"  ") : nullptr;
        typeface::clip(p,address?address+2:measured?"BSSID unavailable":"No network selected",15,155,204,c.muted,typeface::Small);
        int observed=0;for(int i=0;i<count;++i)if(radioObserved(rows[i]))++observed;
        char total[24];snprintf(total,sizeof(total),"%d AP",observed);
        typeface::clip(p,total,250,155,57,c.accent,typeface::Small);
    } else typeface::clip(p,item.detail&&item.detail[0]?item.detail:measured?"Address unavailable":"No device selected",15,155,292,c.muted,typeface::Small);
}
inline void rfidCard(uint16_t*p,const Appearance&a,const char*title,const char*type,const char*uid,const char*sak,const char*atqa,const Status*status=nullptr,const char*footer=nullptr) {
    if(!p)return;workBackground(p,a);header(p,title,a,status);Palette c=palette(a.theme);
    box(p,15,40,290,94,c.focus,7);icon(p,Icon::Rfid,255,45,c.accent);
    textClip(p,type&&type[0]?type:"Unknown card type",26,47,223,c.text);
    textClip(p,"UID",26,69,40,c.muted);textClip(p,uid&&uid[0]?uid:"Not read",26,86,262,c.accent);
    typeface::clip(p,"SAK / PMm",26,112,83,c.muted,typeface::Small);typeface::clip(p,sak&&sak[0]?sak:"--",115,112,170,c.text,typeface::Small);
    typeface::clip(p,"ATQA / System",26,138,104,c.muted,typeface::Small);typeface::clip(p,atqa&&atqa[0]?atqa:"--",141,138,161,c.text,typeface::Small);
    if(footer)typeface::clip(p,footer,18,155,284,c.muted,typeface::Small);
}
}} // namespace delta::ui
