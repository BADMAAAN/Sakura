#pragma once
#include "core.h"
#include <string.h>

namespace delta {
struct DirtyResult { unsigned bands=0, rows=0; };

// Full-width bands keep TFT pushImage contiguous. previous contains LCD-order RGB565,
// before driver byte swapping; only the caller writes it after a successful push.
template<class Present>
inline DirtyResult presentChanged(const uint16_t *current,const uint16_t *previous,Present present) {
    DirtyResult result;
    if(!current)return result;
    if(!previous){present(0,Height);result.bands=1;result.rows=Height;return result;}
    // Two clean rows cost at most 1280 B and avoid another small SPI transaction.
    // This is a bounded policy, not a claim about measured driver setup latency.
    const int gap=2;
    struct Band { uint16_t top,height; } bands[Height/(gap+2)+2];
    int count=0,first=-1,last=-1;
    for(int y=0;y<Height;++y) {
        const bool changed=memcmp(current+y*Width,previous+y*Width,Width*sizeof(uint16_t))!=0;
        if(changed) {
            if(first<0)first=y;
            last=y;
        } else if(first>=0&&y-last>gap) {
            bands[count++]={uint16_t(first),uint16_t(last-first+1)};first=-1;
        }
    }
    if(first>=0)bands[count++]={uint16_t(first),uint16_t(last-first+1)};
    for(int i=0;i<count;++i)result.rows+=bands[i].height;
    // A near-full update or many transactions gets the predictable full push.
    if(result.rows>=unsigned(Height*3/4)||count>12) {
        present(0,Height);result.bands=1;result.rows=Height;return result;
    }
    for(int i=0;i<count;++i)present(bands[i].top,bands[i].height);
    result.bands=unsigned(count);
    return result;
}
} // namespace delta
