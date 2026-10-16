#include <delta/dirty.h>
#include <assert.h>
#include <string.h>

static uint16_t pixels[delta::Width*delta::Height], previous[delta::Width*delta::Height], lcd[delta::Width*delta::Height];
static int calls;
static void push(int top,int height) {
    assert(top>=0&&height>0&&top+height<=delta::Height);
    memcpy(lcd+top*delta::Width,pixels+top*delta::Width,height*delta::Width*sizeof(uint16_t));++calls;
}
static void assertMatch(){assert(memcmp(lcd,pixels,sizeof(lcd))==0);}
static delta::DirtyResult update(bool cold=false) {
    calls=0;auto result=delta::presentChanged(pixels,cold?nullptr:previous,push);assert(int(result.bands)==calls);assertMatch();memcpy(previous,pixels,sizeof(previous));return result;
}
int main() {
    auto r=delta::presentChanged(nullptr,previous,push);assert(!r.rows&&!r.bands&&!calls);
    memset(pixels,0x47,sizeof(pixels));r=update(true);assert(r.rows==170&&r.bands==1);
    r=update();assert(!r.rows&&!r.bands);
    pixels[20*320+4]^=1;r=update();assert(r.rows==1&&r.bands==1);
    pixels[0]^=1;pixels[169*320+319]^=1;r=update();assert(r.rows==2&&r.bands==2);
    pixels[50*320]^=1;pixels[53*320]^=1;r=update();assert(r.rows==4&&r.bands==1);
    pixels[50*320]^=1;pixels[54*320]^=1;r=update();assert(r.rows==2&&r.bands==2);
    for(int y=0;y<170;y+=4)pixels[y*320]^=1;r=update();assert(r.rows==170&&r.bands==1);
    memset(pixels,0,sizeof(pixels));r=update();assert(r.rows==170&&r.bands==1);
    // Caller can invalidate after another renderer owns the LCD.
    memset(lcd,0xff,sizeof(lcd));r=update(true);assert(r.rows==170&&r.bands==1);
    // Alternating buffers remain RGB565, and presenter owns byte swapping.
    previous[169*320+17]=0xabcd;
    calls=0;r=delta::presentChanged(previous,pixels,[](int top,int height){
        memcpy(lcd+top*320,previous+top*320,height*320*sizeof(uint16_t));++calls;
    });assert(r.rows==1&&r.bands==1&&calls==1&&memcmp(lcd,previous,sizeof(lcd))==0);
    // Deterministic sparse edits exercise all band positions without fake timing.
    unsigned state=731;
    memcpy(previous,pixels,sizeof(previous));memcpy(lcd,pixels,sizeof(lcd));
    for(int trial=0;trial<200;++trial){for(int i=0;i<trial%19;++i){state=state*1664525u+1013904223u;pixels[state%(320*170)]^=uint16_t(state>>16);}update();}
}
