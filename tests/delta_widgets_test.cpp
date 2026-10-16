#include "../include/delta/widgets.h"
#include <assert.h>
#include <stdio.h>
#include <string>

static uint16_t guarded[delta::Width*delta::Height+2];
static uint16_t*p=guarded+1;
static void intact(){assert(guarded[0]==0x1234);assert(guarded[delta::Width*delta::Height+1]==0xabcd);}
int main(){
    using namespace delta;using namespace delta::ui;
    guarded[0]=0x1234;guarded[Width*Height+1]=0xabcd;
    Appearance a;Status status;status.time="21:04";status.battery=-1;
    MenuRow rows[]={
        {"Network settings with a deliberately long label", "Connected",true},
        {"Disabled action", "Unavailable",false},
        {"Reduced motion","",true,false,Toggle|Checked},
        {"Folder with files","12 files",true,false,Folder},
        {"Receiver","-61 dBm",true,false,Signal},
        {"Error message","",true,false,Warning},
        {"Sakura \xe6\xa1\x9c and \xf0\x9f\x8c\xb8", "",true}
    };
    assert(measure("\xe6\xa1\x9c")==measure("?"));
    assert(measure("\xf0\x9f\x8c\xb8")==measure("?"));
    assert(firstVisible(7,0)==0);assert(firstVisible(7,3)==1);assert(firstVisible(7,6)==2);
    assert(firstVisible(0,99)==0);assert(firstVisible(9,-20)==0);assert(firstVisible(9,99)==4);
    assert(progressPixels(-1,10,278)==0);assert(progressPixels(10,0,278)==0);
    assert(progressPixels(6,10,278)==166);assert(progressPixels(200,10,278)==278);
    assert(progressPixels(2147483646,2147483647,278)==277);
    for(int theme=0;theme<3;++theme){
        a.theme=Theme(theme);Palette c=palette(a.theme);
        workBackground(p,a,false);for(int i=0;i<Width*Height;++i)assert(p[i]==c.background);
        list(p,a,"Settings",rows,7,6,&status);intact();
        assert(p[145*Width+11]==c.accent); // last row stays inside the five-row viewport
        list(p,a,"Settings",rows,7,0,&status);assert(p[45*Width+11]==c.accent);intact();
        list(p,a,"Empty",nullptr,0,0);intact();
        list(p,a,"Bounds",rows,7,-99);intact();list(p,a,"Bounds",rows,7,999);intact();
        dialog(p,a,"Confirm","First line\nLong input stays inside the screen and wraps without an allocation.\nThird line","Continue","Back",1);intact();
        dialog(p,a,"Error","Failed to read. Existing data is preserved.","OK",nullptr);intact();
        progress(p,a,"Copy","Destination: /sd/a-very-long-file.bin",2147483646,2147483647);intact();
        progress(p,a,"Unknown","Waiting",12,0);intact();
        const char*keys[]={"A","B","C","D","E","F","G","H","I","J","K"};
        keyboard(p,a,"Keyboard","Typed text",keys,11,10);intact();
        keyboard(p,a,"Keyboard","Typed text",nullptr,0,0);intact();
        box(p,-100,-100,500,500,c.text,4);intact();
        textClip(p,"Very long text \xe6\xa1\x9c",-8,-4,100,c.text);intact();
        smallIcon(p,Folder|Signal|Warning,-6,-6,c.text);intact();
    }
    a.theme=Theme::DarkSakura;workBackground(p,a,false);uint16_t bg=p[0];
    MenuRow checked("Choice","",true,true);row(p,a,checked,39,false);
    assert(p[51*Width+289]==palette(a.theme).accent); // Persistent choice mark is independent of focus.
    workBackground(p,a,false);
    textClip(p,"ABCDEFGHIJKLMNOPQRSTUVWXYZ",21,40,63,palette(a.theme).text);
    for(int y=40;y<53;++y)for(int x=84;x<Width;++x)assert(p[y*Width+x]==bg);
    list(nullptr,a,"Null",rows,7,0);dialog(nullptr,a,"Null","Message");progress(nullptr,a,"Null","",0,0);keyboard(nullptr,a,"Null","",nullptr,0,0);
    const char*message="Keep current screen?";char wrapped[128];
    messageLine(message,wrapped,128,measure("Keep current"));assert(std::string(wrapped)=="Keep current");
    messageLine(message,wrapped,128,278);assert(std::string(wrapped)=="screen?");
    message="abcdefgh";messageLine(message,wrapped,128,measure("abcd"));assert(std::string(wrapped)=="abcd"&&std::string(message)=="efgh");
    puts("DELTA widgets: bounds, UTF-8 fallback, viewport, themes, dialogs, progress and null buffer PASS");
}
