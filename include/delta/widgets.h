#pragma once
#include "core.h"
#include "typography.h"
#include <string.h>

// Shared pixels for firmware and native preview. Navigation and actions stay with their callers.
namespace delta { namespace ui {
struct Palette { uint16_t background, text, muted, accent, focus, line, error; };
inline Palette palette(Theme theme) {
    if (theme==Theme::SakuraDawn) return {color(239,228,226),color(48,38,46),color(98,78,93),color(153,62,99),color(222,199,211),color(205,190,199),color(163,43,58)};
    if (theme==Theme::Monochrome) return {color(18,19,22),color(232,232,232),color(159,160,164),color(223,223,226),color(47,48,53),color(62,64,69),color(232,232,232)};
    return {color(2,2,3),color(245,240,244),color(183,167,178),color(235,161,190),color(9,9,13),color(48,29,43),color(247,132,145)};
}
inline int bounded(int value,int low,int high) { return value<low?low:value>high?high:value; }
inline void box(uint16_t*p,int x,int y,int w,int h,uint16_t c,int radius=0,unsigned opacity=255) {
    if(!p||w<=0||h<=0)return;
    radius=bounded(radius,0,(w<h?w:h)/2);
    int l=bounded(x,0,Width),r=bounded(x+w,0,Width),t=bounded(y,0,Height),b=bounded(y+h,0,Height);
    for(int yy=t;yy<b;++yy)for(int xx=l;xx<r;++xx) {
        int dx=xx<x+radius?x+radius-xx-1:xx>=x+w-radius?xx-(x+w-radius):0;
        int dy=yy<y+radius?y+radius-yy-1:yy>=y+h-radius?yy-(y+h-radius):0;
        if(dx*dx+dy*dy<=radius*radius)p[yy*Width+xx]=opacity>=255?c:blend(p[yy*Width+xx],c,opacity);
    }
}
// A missing Unicode glyph is one '?', rather than one '?' per UTF-8 byte.
inline unsigned nextGlyph(const char*&s) {
    return typeface::nextCodepoint(s);
}
inline int measure(const char*s,int maxWidth=Width) {
    return typeface::measure(s,typeface::Body,maxWidth);
}
inline void textClip(uint16_t*p,const char*s,int x,int y,int width,uint16_t c,unsigned opacity=255) {
    typeface::clip(p,s,x,y,width,c,typeface::Body,opacity);
}
struct BackgroundCache { uint16_t* pixels=nullptr; int theme=-1; bool bypass=false; };
inline BackgroundCache& backgroundCache() { static BackgroundCache cache; return cache; }
inline void workBackground(uint16_t*p,const Appearance&a,bool decoration=true) {
    if(!p)return;Palette c=palette(a.theme);
    const auto& cache=backgroundCache();
    if(decoration&&!cache.bypass&&cache.pixels&&cache.theme==int(a.theme)) { memcpy(p,cache.pixels,Width*Height*sizeof(uint16_t));return; }
    if(decoration)renderBackground(p,a);
    else for(int i=0;i<Width*Height;++i)p[i]=c.background;
}
inline void header(uint16_t*p,const char*title,const Appearance&a,const Status*status=nullptr) {
    if(!p)return;Palette c=palette(a.theme);int titleWidth=status&&a.status?191:292;
    textClip(p,title,14,5,titleWidth,c.text);
    if(status)renderStatus(p,a,*status);
    box(p,14,29,292,1,c.line);
    box(p,14,29,27,1,c.accent);
}
// Fullscreen mode hierarchy; compact legacy header stays available for data canvases.
inline void modeHeader(uint16_t*p,const char*title,const char*subtitle,const Appearance&a,const Status*status=nullptr) {
    header(p,title,a,status);
    if(subtitle&&*subtitle)typeface::clip(p,subtitle,14,37,292,palette(a.theme).muted,typeface::Small);
}
enum RowFlag : uint8_t { Toggle=1, Checked=2, Folder=4, Signal=8, Warning=16 };
struct MenuRow {
    const char*label;const char*value;bool enabled;bool selected;uint8_t flags;
    MenuRow(const char*l="",const char*v="",bool e=true,bool s=false,uint8_t f=0):label(l),value(v),enabled(e),selected(s),flags(f){}
};
inline int firstVisible(int count,int selected) {
    return count<=5?0:bounded(selected-2,0,count-5);
}
inline void smallIcon(uint16_t*p,uint8_t flags,int x,int y,uint16_t c) {
    if(!p)return;
    if(flags&Folder){line(p,x,y+3,x+12,y+3,c);line(p,x,y+3,x,y+11,c);line(p,x,y+11,x+12,y+11,c);line(p,x+12,y+11,x+12,y+3,c);line(p,x+1,y+1,x+5,y+1,c);line(p,x+1,y+1,x+1,y+3,c);line(p,x+5,y+1,x+7,y+3,c);}
    if(flags&Signal)for(int i=0;i<4;++i)box(p,x+i*3,y+10-i*2,2,2+i*2,c);
    if(flags&Warning){line(p,x+6,y,x,y+11,c);line(p,x,y+11,x+12,y+11,c);line(p,x+12,y+11,x+6,y,c);line(p,x+6,y+4,x+6,y+7,c);pixel(p,x+6,y+9,c);}
}
inline void row(uint16_t*p,const Appearance&a,const MenuRow&r,int y,bool focus) {
    if(!p)return;Palette c=palette(a.theme);
    if(focus){box(p,11,y,297,23,c.focus,5);box(p,11,y+6,2,11,c.accent,1);}
    uint16_t foreground=r.enabled?c.text:c.muted;
    int left=21;if(r.flags&(Folder|Signal|Warning)){smallIcon(p,r.flags,left,y+5,r.enabled?c.muted:c.line);left+=20;}
    int right=298;
    if(r.selected&&!(r.flags&Toggle)){line(p,289,y+12,292,y+15,c.accent);line(p,292,y+15,298,y+8,c.accent);right=280;}
    if(r.flags&Toggle){box(p,278,y+6,21,11,(r.flags&Checked)?c.accent:c.line,5);box(p,(r.flags&Checked)?289:280,y+8,7,7,(r.flags&Checked)?c.background:c.muted,3);right=268;}
    else if(r.value&&r.value[0]){int w=typeface::measure(r.value,typeface::Small,110);typeface::clip(p,r.value,right-w,y+4,w,c.muted,typeface::Small);right-=w+12;}
    textClip(p,r.label,left,y+2,right-left,foreground,r.enabled?255:160);
}
inline void list(uint16_t*p,const Appearance&a,const char*title,const MenuRow*rows,int count,int selected,const Status*status=nullptr) {
    if(!p)return;workBackground(p,a);header(p,title,a,status);
    if(!rows||count<=0){textClip(p,"No items",21,75,277,palette(a.theme).muted);return;}
    selected=bounded(selected,0,count-1);int first=firstVisible(count,selected);
    for(int i=first;i<count&&i<first+5;++i)row(p,a,rows[i],39+(i-first)*25,i==selected);
    if(count>5){Palette c=palette(a.theme);box(p,314,42,2,119,c.line,1);int h=119*5/count;if(h<6)h=6;box(p,314,42+(119-h)*first/(count-5),2,h,c.muted,1);}
}
inline void modeList(uint16_t*p,const Appearance&a,const char*title,const char*subtitle,const MenuRow*rows,int count,int selected,const Status*status=nullptr,int focusY=-1) {
    if(!p)return;
    workBackground(p,a);header(p,title,a,status);
    const Palette c=palette(a.theme);
    if(!rows||count<=0){textClip(p,"No items",14,83,292,c.muted);return;}
    selected=bounded(selected,0,count-1);const MenuRow &r=rows[selected];
    const auto face=typeface::measure(r.label,typeface::Numeric,1000)<=292?typeface::Numeric:typeface::Body;
    const int width=typeface::measure(r.label,face,292);
    const int offset=focusY<0?0:bounded(focusY-39,0,5);
    typeface::clip(p,r.label,(Width-width)/2,76+offset,292,r.enabled?c.text:c.muted,face,r.enabled?255:160);
    box(p,144,113+offset,32,1,c.accent);
    const char *value=r.value&&*r.value?r.value:r.selected?"Selected":"";
    const int valueWidth=typeface::measure(value,typeface::Small,292);
    typeface::clip(p,value,(Width-valueWidth)/2,124+offset,292,c.muted,typeface::Small);
}
inline void messageLine(const char*&message,char*output,int capacity,int width) {
    if(!output||capacity<1)return;
    int n=0,w=0,spaceIndex=-1;const char*afterSpace=nullptr;
    while(*message==' ')++message;
    while(*message&&*message!='\n'&&n<capacity-1) {
        const char*next=message;unsigned glyph=nextGlyph(next);
        int advance=typeface::glyph(glyph).advance;int bytes=int(next-message);if(w+advance>width||n+bytes>=capacity)break;
        if(glyph==' '){spaceIndex=n;afterSpace=next;}
        while(message<next)output[n++]=*message++;
        w+=advance;
    }
    if(*message==' '){while(*message==' ')++message;}
    else if(*message&&*message!='\n'&&afterSpace){message=afterSpace;n=spaceIndex;}
    else if(*message=='\n')++message;
    else if(!n&&*message)nextGlyph(message);
    output[n]=0;
}
inline void messageLines(uint16_t*p,const char*message,int x,int y,int width,int maxLines,uint16_t c) {
    if(!p||!message)return;
    for(int i=0;*message&&i<maxLines;++i) {
        char lineText[128];messageLine(message,lineText,sizeof(lineText),width);
        textClip(p,lineText,x,y+i*18,width,c);
    }
}
inline void dialog(uint16_t*p,const Appearance&a,const char*title,const char*message,const char*confirm="OK",const char*cancel="Back",int selected=0) {
    if(!p)return;workBackground(p,a);header(p,title,a);Palette c=palette(a.theme);
    messageLines(p,message,21,44,278,4,c.text);
    bool hasCancel=cancel&&cancel[0];int w=hasCancel?136:278;
    box(p,21,137,w,25,selected==0?c.focus:c.background,5);if(selected==0)box(p,21,144,2,11,c.accent,1);
    textClip(p,confirm,30,143,w-18,c.text);
    if(hasCancel){box(p,163,137,136,25,selected!=0?c.focus:c.background,5);if(selected!=0)box(p,163,144,2,11,c.accent,1);textClip(p,cancel,172,143,118,c.text);}
}
inline int progressPixels(int value,int maximum,int width) {
    if(maximum<=0||value<=0||width<=0)return 0;if(value>=maximum)return width;
    return int(int64_t(value)*width/maximum);
}
inline void progress(uint16_t*p,const Appearance&a,const char*title,const char*message,int value,int maximum) {
    if(!p)return;workBackground(p,a);header(p,title,a);Palette c=palette(a.theme);
    messageLines(p,message,21,51,278,3,c.text);box(p,21,128,278,4,c.line,2);box(p,21,128,progressPixels(value,maximum,278),4,c.accent,2);
    int percent=progressPixels(value,maximum,100);char number[5];number[0]=char('0'+percent/100);number[1]=char('0'+percent/10%10);number[2]=char('0'+percent%10);number[3]='%';number[4]=0;
    textClip(p,number+(percent>=100?0:percent>=10?1:2),21,142,60,c.muted);
}
inline void keyboard(uint16_t*p,const Appearance&a,const char*title,const char*value,const char*const*keys,int count,int selected) {
    if(!p)return;workBackground(p,a,false);header(p,title,a);Palette c=palette(a.theme);
    box(p,13,36,294,23,c.focus,4);textClip(p,value,21,41,278,c.text);
    if(!keys||count<=0)return;selected=bounded(selected,0,count-1);
    const int columns=10,visible=40;int first=(selected/columns>=4)?(selected/columns-3)*columns:0;
    for(int i=first;i<count&&i<first+visible;++i){int col=(i-first)%columns,ln=(i-first)/columns,x=13+col*30,y=66+ln*24;
        if(i==selected)box(p,x,y,25,21,c.focus,4);textClip(p,keys[i],x+4,y+4,20,i==selected?c.accent:c.text);
    }
}
}} // namespace delta::ui
