#include "../include/delta/core.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <vector>

int main(int argc,char **argv) {
    using namespace delta;
    static const char *labels[]={"WiFi","Bluetooth","RF","LoRa","Infrared","Ethernet","RFID","Files","Scripts","Clock","Tools","Settings","Favorites"};
    std::vector<uint16_t> one(Width*Height+2,0x1234),two(Width*Height+2,0x1234),background(Width*Height);
    uint16_t *p=&one[1],*q=&two[1];
    Navigation n; Status s; s.time="23:48";s.battery=63;s.wifi=true;s.ble=true;
    Appearance a;a.reducedMotion=true;
    assert(homeTextWidth("Bluetooth")>textWidth("Bluetooth"));
    for(int theme=0;theme<3;++theme) {
        a.theme=Theme(theme);
        renderBackground(background.data(),a);
        for(int i=0;i<13;++i) {
            n.previous=n.selected=i;
            render(p,a,n,Icon(i),labels[i],Icon(i),labels[i],s,1000);
            memcpy(q,background.data(),Width*Height*2);
            renderOverlay(q,a,n,Icon(i),labels[i],Icon(i),labels[i],s,1000);
            assert(memcmp(p,q,Width*Height*2)==0); // Cached composition preserves exact rendering.
            render(q,a,n,Icon(i),labels[i],Icon(i),labels[i],s,90000);
            assert(memcmp(p,q,Width*Height*2)==0); // Reduced Motion freezes decoration.
            assert(homeTextWidth(labels[i])<=104); // Label fits the one-slot negative-space region.
        }
        assert(one.front()==0x1234&&one.back()==0x1234&&two.front()==0x1234&&two.back()==0x1234);
    }
    a.theme=Theme::DarkSakura;a.reducedMotion=false;a.petals=3;a.glitch=true;
    n.select(1,1,1000);
    for(unsigned ms=1000;ms<1200;ms+=7) {
        render(p,a,n,Icon::Bluetooth,"Bluetooth",Icon::Wifi,"WiFi",s,ms);
        renderBackground(q,a);renderOverlay(q,a,n,Icon::Bluetooth,"Bluetooth",Icon::Wifi,"WiFi",s,ms);
        assert(memcmp(p,q,Width*Height*2)==0);
    }
    a.reducedMotion=true;a.glitch=false;
    render(p,a,n,Icon::Clock,"Clock",Icon::Wifi,"WiFi",s,10000,true);
    s.time="01:23";
    render(q,a,n,Icon::Clock,"Clock",Icon::Wifi,"WiFi",s,10000,true);
    assert(memcmp(p,q,Width*Height*2)!=0); // Actual clock text still changes with Reduced Motion.
    assert(one.front()==0x1234&&one.back()==0x1234&&two.front()==0x1234&&two.back()==0x1234);
    for(unsigned i=0;i<sizeof(artwork::background);++i)assert(artwork::background[i]<64);
    assert(sizeof(artwork::background)==54400&&sizeof(artwork::palette0)==128);
    a.reducedMotion=false;a.glitch=false;s.time="23:48";n.previous=n.selected=0;
    render(p,a,n,Icon::Wifi,"WiFi",Icon::Wifi,"WiFi",s,5300);
    if(argc>1) {
        FILE *out=fopen(argv[1],"wb");assert(out);
        fprintf(out,"P6\n320 170\n255\n");
        for(int i=0;i<Width*Height;++i){uint16_t c=p[i];unsigned char rgb[]={static_cast<unsigned char>((c>>11)*255/31),static_cast<unsigned char>(((c>>5)&63)*255/63),static_cast<unsigned char>((c&31)*255/31)};fwrite(rgb,1,3,out);}
        fclose(out);
    }
    puts("Cyber Home: 13 modes, cached composition, themes, AA text, clock, Reduced Motion and frame guards PASS");
}
