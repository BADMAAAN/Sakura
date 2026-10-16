#include "../include/delta/core.h"
#include <assert.h>
#include <string.h>
int main() {
    delta::Navigation n;assert(n.select(1,1,100));assert(n.selected==1&&n.previous==0);assert(!n.select(1,1,110));
    delta::Appearance a;
    auto tr=delta::transition(a,n,100);assert(!tr.outgoing&&tr.opacity==210&&tr.offset==4);
    tr=delta::transition(a,n,180);assert(!tr.outgoing&&tr.opacity>=230);
    tr=delta::transition(a,n,260);assert(!tr.outgoing&&tr.opacity==255&&tr.offset==0);
    assert(n.select(2,1,120));assert(n.select(1,-1,125));assert(n.selected==1&&n.previous==2&&n.direction==-1&&n.changedAt==125);
    a.reducedMotion=true;tr=delta::transition(a,n,125);assert(!tr.outgoing&&tr.opacity==255&&tr.offset==0);
    uint16_t one[54400],two[54400];delta::Status s;
    delta::render(one,a,n,delta::Icon::Bluetooth,"Bluetooth",delta::Icon::Radio,"RF",s,125);
    delta::render(two,a,n,delta::Icon::Bluetooth,"Bluetooth",delta::Icon::Radio,"RF",s,123456);
    assert(memcmp(one,two,sizeof(one))==0); // Reduced Motion freezes all decoration.
    a.reducedMotion=false;a.petals=0;n.previous=n.selected;
    delta::render(one,a,n,delta::Icon::Bluetooth,"Bluetooth",delta::Icon::Radio,"RF",s,1);
    delta::render(two,a,n,delta::Icon::Bluetooth,"Bluetooth",delta::Icon::Radio,"RF",s,10000);
    assert(memcmp(one,two,sizeof(one))==0);
    a.petals=3;
    delta::render(two,a,n,delta::Icon::Bluetooth,"Bluetooth",delta::Icon::Radio,"RF",s,10000);
    assert(memcmp(one,two,sizeof(one))!=0);
    // The newest target is visible from the first frame, including rapid reversals.
    n.select(2,1,400);
    for(unsigned ms=400;ms<581;++ms){tr=delta::transition(a,n,ms);assert(!tr.outgoing);assert(tr.opacity>=210&&tr.opacity<=255);}
    n.changedAt=0xfffffff0U;tr=delta::transition(a,n,0x90U);assert(!tr.outgoing&&tr.opacity==255);
    return 0;
}
