#include <delta/section_ui.h>
#include <cassert>
#include <cstring>
#include <vector>
int main() {
    using namespace delta; using namespace delta::ui;
    assert(color(0,0,0)==0 && color(255,255,255)==65535);
    assert(color(255,0,0)==0xf800 && color(0,255,0)==0x07e0 && color(0,0,255)==0x001f);
    assert(blend(0,0xf800,255)==0xf800 && blend(0x1234,0xffff,0)==0x1234);
    Appearance a; a.reducedMotion=true;
    a.petals=3;a.glitch=true;
    const auto limited=adaptiveAppearance(a,true);
    assert(limited.petals==1&&!limited.glitch&&a.petals==3&&a.glitch);
    assert(adaptiveAppearance(a,false).petals==3);
    a.petals=0;assert(adaptiveAppearance(a,true).petals==0);
    std::vector<uint16_t> one(Width*Height),two(Width*Height);
    renderBackground(one.data(),a); workBackground(two.data(),a);
    assert(!memcmp(one.data(),two.data(),one.size()*2));
    unsigned darkPixels=0;
    for(int i=0;i<Width*Height;++i) {
        const uint16_t original=artwork::palette0[artwork::background[i]];
        const int r=(original>>11)&31,g=(original>>5)&63,b=original&31;
        if(r<=2&&g<=4&&b<=2){assert(one[i]==0);++darkPixels;}
        else assert(one[i]==original);
    }
    assert(darkPixels>1000); // Quantized shadows, not a replacement image or screen overlay.
    // Only the chosen mode belongs on screen: editing another label changes no pixel.
    MenuRow rows[]={{"Selected mode"},{"Other mode"},{"Back"}};
    modeList(one.data(),a,"Wi-Fi","",rows,3,0);
    rows[1].label="Unselected must remain invisible";
    modeList(two.data(),a,"Wi-Fi","",rows,3,0);
    assert(!memcmp(one.data(),two.data(),one.size()*2));
    wifiMain(one.data(),a,rows,3,0); rows[1].label="Still invisible";
    wifiMain(two.data(),a,rows,3,0);assert(!memcmp(one.data(),two.data(),one.size()*2));
    Status s; s.battery=63; s.charging=true; s.time="12:34";
    for(int theme=0;theme<3;++theme) {
        a.theme=Theme(theme); renderBackground(one.data(),a);renderBackground(two.data(),a);
        header(one.data(),"Wi-Fi",a,&s);modeHeader(two.data(),"Settings","",a,&s);
        for(int y=4;y<21;++y)for(int x=288;x<318;++x)assert(one[y*Width+x]==two[y*Width+x]);
    }
}
