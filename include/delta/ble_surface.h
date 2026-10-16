#pragma once
#include "widgets.h"
#include <stdio.h>

// BLE-specific pixels only. The firmware owns selection, settings and radio state.
namespace delta { namespace ui {
inline void bleMark(uint16_t*p,int x,int y,uint16_t c) {
    line(p,x+7,y,x+7,y+24,c);line(p,x+7,y,x+15,y+7,c);
    line(p,x+15,y+7,x,y+19,c);line(p,x,y+5,x+15,y+17,c);
    line(p,x+15,y+17,x+7,y+24,c);
}
inline void bleList(uint16_t*p,const Appearance&a,const char*title,const char*const*labels,
                    int visible,int first,int total,int selected,const Status*status=nullptr) {
    if(!p)return;
    const int available=labels?bounded(visible,0,4):0;
    const int local=selected-first;
    MenuRow row(local>=0&&local<available?labels[local]:"No items");
    modeList(p,a,title,"",&row,1,0,status);
}
inline void bleFields(uint16_t*p,const Appearance&a,const char*const*values,int cursor,int editing,int startY=62) {
    if(!p)return;Palette c=palette(a.theme);
    const char*labels[]={"Advertising","Gap","TX power","MAC rotation"};
    for(int i=0;i<4;++i){int y=startY+i*19;
        if(cursor==i){box(p,13,y,294,18,c.focus,3);box(p,13,y+4,2,10,c.accent);}
        textClip(p,labels[i],22,y+1,132,cursor==i?c.text:c.muted);
        const char*value=values&&values[i]?values[i]:"--";
        int w=typeface::measure(value,typeface::Small,134);typeface::clip(p,value,296-w,y+3,w,editing==i?c.accent:c.text,typeface::Small);
        if(editing==i)box(p,296-w,y+18,w,1,c.accent);
    }
}
inline void bleConfig(uint16_t*p,const Appearance&a,const char*device,const char*const*values,
                      int cursor,int editing,const Status*status=nullptr) {
    if(!p)return;workBackground(p,a);modeHeader(p,"BLE SPAM",device,a,status);Palette c=palette(a.theme);
    bleFields(p,a,values,cursor,editing);
    box(p,13,144,294,22,cursor==4?c.accent:c.focus,3);
    if(editing>=0)typeface::clip(p,"Select: save / Back: undo",22,148,274,c.accent,typeface::Small);
    else textClip(p,"START",22,146,274,cursor==4?c.background:c.text);
}
inline void bleRunning(uint16_t*p,const Appearance&a,const char*device,const char*const*values,
                       int cursor,int editing,uint32_t sent,float packets,bool blink,const Status*status=nullptr) {
    if(!p)return;workBackground(p,a);modeHeader(p,"BLE SPAM",device,a,status);Palette c=palette(a.theme);char stats[64];
    snprintf(stats,sizeof(stats),"Sent %lu    %.1f pkt/s",(unsigned long)sent,double(packets));
    typeface::clip(p,stats,22,61,276,c.accent,typeface::Small);box(p,304,64,4,4,blink?c.accent:c.line,2);
    bleFields(p,a,values,cursor,editing,84);
}
inline void blePrompt(uint16_t*p,const Appearance&a,const char*title,const char*first,const char*second,
                      int selected,const char*message=nullptr,const Status*status=nullptr) {
    if(!p)return;workBackground(p,a);modeHeader(p,"BLE SPAM",title,a,status);Palette c=palette(a.theme);
    if(message)typeface::clip(p,message,21,61,278,c.muted,typeface::Small);
    const char*labels[]={first,second};
    for(int i=0;i<2;++i){int y=84+i*32;if(i==selected){box(p,13,y,294,29,c.focus,3);box(p,13,y+7,2,15,c.accent);}
        textClip(p,labels[i],24,y+5,270,c.text);}
    typeface::clip(p,"Select: confirm    Back: return",21,155,278,c.muted,typeface::Small);
}
}} // namespace delta::ui
