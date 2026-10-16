#include "../include/delta/settings.h"
#include <assert.h>
#include <string.h>
int main() {
    delta::Settings s;
    s.theme=2;s.quality=2;s.petals=3;s.transitionMs=180;s.glitch=true;s.reducedMotion=true;s.status=false;s.icons=false;s.favorites=0x8005;
    uint8_t data[delta::SettingsBytes];
    delta::encodeSettings(s,data);
    delta::Settings out;
    assert(delta::decodeSettings(data,sizeof(data),out));
    assert(out.theme==2&&out.quality==2&&out.petals==3&&out.transitionMs==180);
    assert(out.glitch&&out.reducedMotion&&!out.status&&!out.icons&&out.favorites==0x8005);
    uint8_t v1[12]={'D','L','T',1,1,0,0,12,140,0,0,0};
    v1[11]=delta::settingsChecksum(v1,11);
    assert(delta::decodeSettings(v1,sizeof(v1),out));
    assert(out.theme==1&&out.transitionMs==140&&out.status&&out.icons&&out.favorites==0);
    data[3]=99;data[15]=delta::settingsChecksum(data,15);
    assert(!delta::decodeSettings(data,sizeof(data),out));
    assert(out.theme==0&&out.favorites==0&&out.transitionMs==160);
    delta::encodeSettings(s,data);data[4]=3;data[15]=delta::settingsChecksum(data,15);
    assert(!delta::decodeSettings(data,sizeof(data),out));
    delta::encodeSettings(s,data);data[8]=159;data[15]=delta::settingsChecksum(data,15);
    assert(!delta::decodeSettings(data,sizeof(data),out));
    delta::encodeSettings(s,data);data[13]=1;data[15]=delta::settingsChecksum(data,15);
    assert(!delta::decodeSettings(data,sizeof(data),out));
    delta::encodeSettings(s,data);data[13]^=1;
    assert(!delta::decodeSettings(data,sizeof(data),out));
    assert(!delta::decodeSettings(data,sizeof(data)-1,out));
    assert(!delta::decodeSettings(nullptr,0,out));
    for(unsigned i=0;i<sizeof(data);++i){delta::encodeSettings(s,data);data[i]^=1;assert(!delta::decodeSettings(data,sizeof(data),out));}
    return 0;
}
