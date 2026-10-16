#pragma once
#include <stddef.h>
#include <stdint.h>

namespace delta {
static const size_t SettingsBytes = 16;
struct Settings {
    uint8_t theme = 0, quality = 1, petals = 2;
    uint16_t transitionMs = 160;
    bool glitch = false, reducedMotion = false, status = true, icons = true;
    uint32_t favorites = 0;
};
inline uint8_t settingsChecksum(const uint8_t *data, size_t count) {
    uint8_t crc = 0;
    for (size_t i = 0; i < count; ++i) {
        crc ^= data[i];
        for (unsigned bit = 0; bit < 8; ++bit)
            crc = (crc & 0x80) ? uint8_t((crc << 1) ^ 7) : uint8_t(crc << 1);
    }
    return crc;
}
inline void encodeSettings(const Settings &s, uint8_t (&data)[SettingsBytes]) {
    data[0]='D'; data[1]='L'; data[2]='T'; data[3]=2;
    data[4]=s.theme; data[5]=s.quality; data[6]=s.petals;
    data[7]=(s.glitch?1:0)|(s.reducedMotion?2:0)|(s.status?4:0)|(s.icons?8:0);
    data[8]=uint8_t(s.transitionMs); data[9]=uint8_t(s.transitionMs >> 8);
    for (unsigned i=0; i<4; ++i) data[10+i]=uint8_t(s.favorites >> (i*8));
    data[14]=0; data[15]=settingsChecksum(data,15);
}
inline bool decodeSettings(const uint8_t *data, size_t count, Settings &out) {
    out=Settings{};
    if (!data || (count!=12 && count!=SettingsBytes)) return false;
    if (data[0]!='D' || data[1]!='L' || data[2]!='T') return false;
    if ((data[3]!=1 || count!=12) && (data[3]!=2 || count!=SettingsBytes)) return false;
    if (settingsChecksum(data,count-1)!=data[count-1]) return false;
    const uint16_t duration=uint16_t(data[8])|(uint16_t(data[9])<<8);
    if (data[4]>2 || data[5]>2 || data[6]>3 || data[7]>15 ||
        (duration!=140 && duration!=160 && duration!=180)) return false;
    if ((data[3]==1 && data[10]!=0) || (data[3]==2 && (data[12]!=0 || data[13]!=0 || data[14]!=0))) return false;
    out.theme=data[4]; out.quality=data[5]; out.petals=data[6]; out.transitionMs=duration;
    out.glitch=data[7]&1; out.reducedMotion=data[7]&2; out.status=data[7]&4; out.icons=data[7]&8;
    // Version 1 had no favorites. Migrate in memory; loading never writes NVS.
    if (data[3]==2) for (unsigned i=0; i<4; ++i) out.favorites|=uint32_t(data[10+i])<<(i*8);
    return true;
}
} // namespace delta
