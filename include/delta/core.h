#pragma once
#include <stdint.h>
#include <math.h>
#include <stdlib.h>
#include "artwork.h"
#include "typography_data.h"

namespace delta {
static const int Width = 320, Height = 170;
enum class Theme : uint8_t { DarkSakura, SakuraDawn, Monochrome };
enum class Icon : uint8_t { Wifi, Bluetooth, Radio, LoRa, Infrared, Ethernet, Rfid, Folder, Script, Clock, Tools, Settings, Favorites };
struct Appearance {
    Theme theme = Theme::DarkSakura;
    uint8_t quality = 1, petals = 2;
    uint16_t transitionMs = 160;
    bool glitch = false, reducedMotion = false, status = true, icons = true;
};
inline Appearance adaptiveAppearance(const Appearance &saved, bool overloaded) {
    Appearance frame = saved;
    if (overloaded) { if (frame.petals > 1) frame.petals = 1; frame.glitch = false; }
    return frame;
}
struct Status {
    int battery = -1;
    bool charging = false, wifi = false, ble = false;
    const char *time = "";
};
struct Navigation {
    int selected = 0, previous = 0;
    uint32_t changedAt = 0;
    int direction = 1;
    bool select(int next, int dir, uint32_t now) {
        if (next == selected) return false;
        previous = selected; selected = next; direction = dir < 0 ? -1 : 1; changedAt = now;
        return true;
    }
};
inline uint16_t color(int r, int g, int b) { return uint16_t((r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)); }
inline uint16_t blend(uint16_t bg, uint16_t fg, unsigned a) {
    if (a >= 255) return fg;
    unsigned ia = 255-a;
    return uint16_t((((((bg>>11)&31)*ia+((fg>>11)&31)*a)/255)<<11) |
                    (((((bg>>5)&63)*ia+((fg>>5)&63)*a)/255)<<5) |
                    (((bg&31)*ia+(fg&31)*a)/255));
}
inline void pixel(uint16_t *p, int x, int y, uint16_t c, unsigned a=255) {
    if (unsigned(x)<Width && unsigned(y)<Height) p[y*Width+x]=blend(p[y*Width+x],c,a);
}
inline void line(uint16_t *p,int x0,int y0,int x1,int y1,uint16_t c) {
    int dx=x1>x0?x1-x0:x0-x1, sx=x0<x1?1:-1;
    int dy=-(y1>y0?y1-y0:y0-y1), sy=y0<y1?1:-1, e=dx+dy;
    for (;;) { pixel(p,x0,y0,c);if(x0==x1&&y0==y1)break;int e2=2*e;if(e2>=dy){e+=dy;x0+=sx;}if(e2<=dx){e+=dx;y0+=sy;} }
}
inline uint8_t alpha(const uint8_t *data, unsigned i) { return (i&1)?data[i>>1]&15:data[i>>1]>>4; }
inline int textWidth(const char *s,int scale=1) {
    int width=0;if(s)for(;*s;++s){unsigned ch=uint8_t(*s);if(ch<32||ch>126)ch='?';width+=artwork::glyphAdvance[ch-32];}
    return width*scale;
}
inline void text(uint16_t *p,const char *s,int x,int y,uint16_t c,unsigned opacity=255,int scale=1) {
    if(!s)return;
    for(;*s;++s) {
        unsigned ch=uint8_t(*s);if(ch<32||ch>126)ch='?';
        for(int yy=0;yy<13;++yy)for(int xx=0;xx<12;++xx) {
            unsigned a=alpha(artwork::glyphAlpha,(ch-32)*156+yy*12+xx)*17*opacity/255;
            if(a)for(int sy=0;sy<scale;++sy)for(int sx=0;sx<scale;++sx)pixel(p,x+xx*scale+sx,y+yy*scale+sy,c,a);
        }
        x+=artwork::glyphAdvance[ch-32]*scale;
    }
}
// Home mode labels are ASCII. Reuse the native AA atlas without a core/typography include cycle.
inline int homeTextWidth(const char *s, unsigned face=0) {
    int width=0;
    if(s)for(;*s;++s) { unsigned ch=uint8_t(*s); if(ch<32||ch>126)ch='?'; width+=typeface::data::glyphs[face<=2?face:0][ch-32].advance; }
    return width;
}
inline void homeText(uint16_t *p,const char *s,int x,int y,uint16_t c,unsigned opacity=255,unsigned face=0) {
    if(!s||!p||!opacity)return;
    for(;*s;++s) {
        unsigned ch=uint8_t(*s);if(ch<32||ch>126)ch='?';
        const typeface::data::Glyph &g=typeface::data::glyphs[face<=2?face:0][ch-32];
        for(int yy=0;yy<g.height;++yy)for(int xx=0;xx<g.width;++xx) {
            unsigned a=alpha(typeface::data::coverage,g.offset+yy*g.width+xx)*17*opacity/255;
            if(a)pixel(p,x+g.x+xx,y+g.y+yy,c,a);
        }
        x+=g.advance;
    }
}
inline void icon(uint16_t *p,Icon which,int x,int y,uint16_t c,unsigned opacity=255) {
    unsigned offset=unsigned(which)*1600;if(offset>=13*1600)return;
    for(int yy=0;yy<40;++yy)for(int xx=0;xx<40;++xx){unsigned a=alpha(artwork::iconAlpha,offset+yy*40+xx)*17*opacity/255;if(a)pixel(p,x+xx,y+yy,c,a);}
}
inline float smooth(float t) { return t*t*(3-2*t); }
struct Transition { bool outgoing; unsigned opacity; int offset; };
inline Transition transition(const Appearance &a,const Navigation &n,uint32_t now) {
    unsigned duration=a.transitionMs<140?140:(a.transitionMs>180?180:a.transitionMs);
    uint32_t elapsed=now-n.changedAt;
    if(a.reducedMotion||n.previous==n.selected||elapsed>=duration)return {false,255,0};
    float t=float(elapsed)/duration;
    float u=smooth(t);
    // The newest encoder selection is visible on its first frame; no outgoing wait.
    return {false,unsigned(210+45*u),int(4*(1-u))*n.direction};
}
inline uint32_t hash(uint32_t v) {v^=v>>16;v*=0x7feb352dU;v^=v>>15;v*=0x846ca68bU;return v^(v>>16);}
inline void renderBackground(uint16_t *p,const Appearance &a) {
    const uint16_t *palette=a.theme==Theme::SakuraDawn?artwork::palette1:a.theme==Theme::Monochrome?artwork::palette2:artwork::palette0;
    for(int i=0;i<Width*Height;++i){
        uint16_t c=palette[artwork::background[i]];
        const int r=(c>>11)&31,g=(c>>5)&63,b=c&31;
        // Neutralize reference-image noise below ~16/255; retain every brighter scene colour.
        if(a.theme==Theme::DarkSakura && r<=2 && g<=4 && b<=2)c=0;
        p[i]=c;
    }
}
inline void renderStatus(uint16_t *p,const Appearance &a,const Status &s) {
    if(!p||!a.status)return;
    const bool mono=a.theme==Theme::Monochrome, dawn=a.theme==Theme::SakuraDawn;
    const uint16_t ink=mono?color(223,223,226):dawn?color(153,62,99):color(235,161,190);
    const uint16_t muted=mono?color(159,160,164):dawn?color(98,78,93):color(183,167,178);
    if(s.time&&s.time[0])homeText(p,s.time,219,4,muted,255,1);
    if(s.wifi){line(p,274,11,277,8,muted);line(p,277,8,280,11,muted);pixel(p,277,13,muted);}
    if(s.ble){line(p,286,6,286,15,muted);line(p,283,8,289,12,muted);line(p,289,12,286,15,muted);line(p,283,13,289,9,muted);line(p,289,9,286,6,muted);}
    if(s.battery>=0&&s.battery<=100) {
        line(p,299,7,313,7,muted);line(p,299,7,299,14,muted);line(p,313,7,313,14,muted);line(p,299,14,313,14,muted);line(p,315,9,315,12,muted);
        for(int y=9;y<13;++y)for(int x=301;x<301+s.battery*11/100;++x)pixel(p,x,y,muted);
    }
    if(s.charging){line(p,294,6,291,11,ink);line(p,291,11,295,11,ink);line(p,295,11,292,16,ink);}
    if(s.battery<0||s.battery>100)homeText(p,"?",302,4,muted,255,1);
}
// Caller restores the cached background before drawing this moving foreground.
inline void renderOverlay(uint16_t *p,const Appearance &a,const Navigation &n,Icon current,const char *label,
                          Icon previous,const char *previousLabel,const Status &s,uint32_t now,bool ambient=false) {
    const bool mono=a.theme==Theme::Monochrome;
    uint16_t ink=mono?color(231,231,231):color(240,191,207);
    if(!a.reducedMotion && a.petals) {
        int count=a.petals==1?7:a.petals==2?14:22;
        if(a.quality==0)count=(count+1)/2;
        else if(a.quality>=2)count+=2;
        for(int i=0;i<count;++i) {
            uint32_t seed=hash(unsigned(i)+731),life=10000+seed%13000;
            float t=float((now+seed%life)%life)/life;
            float phase=float((seed>>8)%628)/100;
            float angle=phase+float(now%60000)*(.0006f+.0002f*(i%3));
            int x=int((seed%217)+t*23+sin(t*9+phase)*(4+i%5));
            int y=int(t*194)-12;
            float ca=cos(angle),sa=sin(angle);
            // Rotating tapered petal, with a quiet distant layer.
            int size=i%3==0?3:2;unsigned opacity=i%3==0?190:115;
            for(int v=-size;v<=size;++v)for(int u=-1;u<=1;++u)if(abs(v)+abs(u)<=size)
                pixel(p,x+int(u*ca-v*sa),y+int(u*sa+v*ca*.55f),ink,opacity);
        }
    }
    if(a.glitch&&!a.reducedMotion&&now%19000<50) {
        int y=48+(now/19000*37)%63;
        for(int x=12;x<154;++x)p[y*Width+x]=p[y*Width+x+2];
    }
    renderStatus(p,a,s);
    if(ambient) {
        const char *clock=s.time&&s.time[0]?s.time:"--:--";
        homeText(p,clock,268-homeTextWidth(clock,2)/2,71,ink,255,2);
        return;
    }
    Transition tr=transition(a,n,now);
    Icon shown=tr.outgoing?previous:current;const char *shownLabel=tr.outgoing?previousLabel:label;
    if(a.icons)icon(p,shown,248,62+tr.offset,ink,tr.opacity);
    homeText(p,shownLabel,268-homeTextWidth(shownLabel)/2,(a.icons?112:86)+tr.offset,ink,tr.opacity);
}
inline void render(uint16_t *p,const Appearance &a,const Navigation &n,Icon current,const char *label,
                   Icon previous,const char *previousLabel,const Status &s,uint32_t now,bool ambient=false) {
    renderBackground(p,a);
    renderOverlay(p,a,n,current,label,previous,previousLabel,s,now,ambient);
}
} // namespace delta
