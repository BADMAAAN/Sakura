"""Compile the production keyboard actions, canvas painter and scroll implementation."""
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(text, signature):
    start = text.index(signature)
    opening = text.index('{', start)
    end, depth = opening + 1, 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


class DeltaInputTests(unittest.TestCase):
    def test_real_actions_canvas_and_scrolling(self):
        keyboard = (ROOT / 'src/core/mykeyboard.cpp').read_text(encoding='utf-8')
        header = (ROOT / 'src/core/scrollableTextArea.h').read_text(encoding='utf-8')
        scrolling = (ROOT / 'src/core/scrollableTextArea.cpp').read_text(encoding='utf-8')
        strip_includes = lambda text: re.sub(r'^#include.*$', '', text, flags=re.M)
        source = r'''
#include <cassert>
#include <string>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <delta/widgets.h>
#define HAS_1_BUTTON 0
#define HAS_ENCODER 1
#define portTICK_PERIOD_MS 1
#define FP 1
#define FM 2
#define LW 6
#define LH 8
#define BORDER_PAD_X 10
#define BORDER_PAD_Y 28
bool strictUtf8Tail=false;
class String : public std::string {
public:
    using std::string::string;
    String(const std::string&s):std::string(s){}
    String(int n):std::string(std::to_string(n)){}
    String(unsigned n):std::string(std::to_string(n)){}
    String(size_t n):std::string(std::to_string(n)){}
    unsigned length()const{return size();}
    bool isEmpty()const{return empty();}
    void remove(unsigned start,unsigned length=~0u){
        if(strictUtf8Tail&&start==0&&length<size())assert((uint8_t(at(length))&0xc0)!=0x80);
        if(start<size())erase(start,length);
    }
    String substring(unsigned start,unsigned end=~0u)const{
        start=std::min(start,(unsigned)size());end=std::min(end,(unsigned)size());
        return substr(start,end>start?end-start:0);
    }
    bool endsWith(const char*s)const{std::string suffix(s);return size()>=suffix.size()&&compare(size()-suffix.size(),suffix.size(),suffix)==0;}
};
String repeatString(int count,const char*s){String out;while(count-->0)out+=s;return out;}
int tftWidth=320,tftHeight=170,max_FP_size=51,max_FM_size=25;
struct {uint16_t priColor=1,bgColor=0;}fluxConfig;
struct Display {
    int size=1, cursorX=0,cursorY=28;
    void setTextColor(int,int=0){}
    void setTextSize(int n){size=n;}
    int getTextSize(){return size;}
    void fillRect(int,int,int,int,int){}
    void setCursor(int x,int y){cursorX=x;cursorY=y;}
    int getCursorX(){return cursorX;}int getCursorY(){return cursorY;}
    void print(const char*){}void print(char){}
    int textWidth(const char*,int){return 6;}
    int fontHeight(){return 8;}
    void drawString(const String&,int,int){}
}tft;
int getComplementaryColor2(int){return 2;}
void drawMainBorder(){}void printTitle(const String&){}
bool SelPress=false,EscPress=false,PrevPress=false,NextPress=false,UpPress=false,DownPress=false;
bool quitAtYield=false;
int32_t rotary=0;
int32_t drainRotarySteps(){int32_t result=rotary;rotary=0;return result;}
void vTaskDelay(int){}
bool check(bool key){return key;}void yield(){if(quitAtYield)SelPress=true;}void delay(int){}
struct File{bool available(){return false;}String readStringUntil(char){return "";}};
uint16_t pixels[320*170+2];
delta::Appearance appearance;delta::Status status;
bool available=true;int presents=0;
uint16_t*deltaUiCanvas(bool clear=true,bool decoration=true){
    if(!available)return nullptr;
    if(clear)delta::ui::workBackground(pixels+1,appearance,decoration);
    return pixels+1;
}
void deltaUiPresent(int=0,int=0,int=320,int=170){++presents;}
const delta::Appearance&deltaUiAppearance(){return appearance;}
const delta::Status&deltaUiStatus(){return status;}
#ifdef FLUX_DELTA_BUILD
static bool deltaKeyboardActive=true;
#endif
'''
        source += strip_includes(header) + strip_includes(scrolling)
        for signature in ('bool handleDelete(', 'bool handleCharacterAdd(', 'bool handleSpaceAdd('):
            source += function(keyboard, signature)
        source += '\nenum KeyboardAction { KEYBOARD_CONTINUE, KEYBOARD_OK, KEYBOARD_CANCEL, KEYBOARD_REDRAW };\n'
        source += function(keyboard, 'KeyboardAction handleKeyboardSelection(')
        source += '\n#ifdef FLUX_DELTA_BUILD\ntemplate <int KeyboardHeight, int KeyboardWidth>\n'
        source += function(keyboard, 'static bool drawDeltaKeyboard(')
        source += '\n#endif\n'
        source += r'''
int main(){
    int x=0,y=0,cx=0,cy=0;bool caps=false,direction=true;
    String value="";
    assert(!handleDelete(value,cx,cy));
    assert(handleCharacterAdd(value,'a',cx,cy,2,true));
    assert(value=="a"); // Masking must never replace actual input.
    assert(handleCharacterAdd(value,'b',cx,cy,2,true));
    assert(!handleCharacterAdd(value,'c',cx,cy,2,true)&&value=="ab");
    assert(handleDelete(value,cx,cy)&&value=="a");
    y=-1;x=1;
    assert(handleKeyboardSelection(x,y,value,caps,direction,cx,cy,4,' ',false)==KEYBOARD_REDRAW&&caps);
    x=3;
    assert(handleKeyboardSelection(x,y,value,caps,direction,cx,cy,4,' ',false)==KEYBOARD_REDRAW&&value=="a ");
    x=0;assert(handleKeyboardSelection(x,y,value,caps,direction,cx,cy,4,' ',false)==KEYBOARD_OK);
    x=4;assert(handleKeyboardSelection(x,y,value,caps,direction,cx,cy,4,' ',false)==KEYBOARD_CANCEL&&value=="\x1b");
    ScrollableTextArea area("INPUT CHECK");
    assert(area.getLine(0)==""&&area.getLine(size_t(-1))=="");
    area.scrollDown();area.scrollToLine(size_t(-1));assert(area.firstVisibleLine==0);
    area.addLine("");area.addLine("short");area.draw();
    area.scrollDown();area.scrollToLine(size_t(-1));assert(area.firstVisibleLine==0);
    area.clear();area.addLine(String(600,'W'));
    size_t total=0;
    for(const auto&line:area.linesBuffer){assert(!line.empty());total+=line.size();
#ifdef FLUX_DELTA_BUILD
        assert(delta::ui::measure(line.c_str(),10000)<=286);
#endif
    }
    assert(total==600);
    area.clear();for(int i=0;i<50;++i)area.addLine(String(i));area.draw();
    const size_t visible=area.lastVisibleLine;
    assert(visible>0&&visible<50);
    area.scrollDown();assert(area.firstVisibleLine==1); // one detent, one line
    area.scrollUp();assert(area.firstVisibleLine==0);
    area.scrollToLine(size_t(-1));assert(area.firstVisibleLine==50-visible);
    area.scrollDown();assert(area.firstVisibleLine==50-visible);
    area.draw();assert(area.lastVisibleLine==50);
    area.scrollToLine(0);SelPress=false;quitAtYield=true;rotary=INT32_MIN;area.show();
    assert(area.firstVisibleLine==50-visible); // One extreme burst clamps in bounded work.
    SelPress=false;rotary=INT32_MAX;area.show();assert(area.firstVisibleLine==0);
    SelPress=false;quitAtYield=false;
    area.linesBuffer.resize(1);area.draw(true);assert(area.firstVisibleLine==0&&area.lastVisibleLine==1);
    area.clear();area.draw(true);assert(area.firstVisibleLine==0&&area.lastVisibleLine==0);
    ScrollableTextArea narrow(1,0,0,0,0,false,true);narrow.addLine("abcdef");
    assert(narrow.getMaxLines()==6); // no zero-width/indent loop
    area.addLine("old");area.scrollToLine(999);area.fromFile(File());assert(area.getMaxLines()==0&&area.firstVisibleLine==0);
#ifdef FLUX_DELTA_BUILD
    char keys[4][12][2]={};for(int i=0;i<4;++i)for(int j=0;j<12;++j){keys[i][j][0]='a'+j;keys[i][j][1]='A'+j;}
    pixels[0]=pixels[320*170+1]=0xabcd;
    const String secret="private-value-which-is-longer-than-a-single-display-value-field-123";
    for(int row=-1;row<4;++row)for(int column=0;column<(row<0?5:12);++column){
        assert((drawDeltaKeyboard<4,12>(secret,100,"Password",keys,true,true,true,column,row,5)));
        assert(pixels[0]==0xabcd&&pixels[320*170+1]==0xabcd);
    }
    assert(secret=="private-value-which-is-longer-than-a-single-display-value-field-123");
    String unicode;for(int i=0;i<40;++i)unicode+="\xd0\xaf";
    strictUtf8Tail=true;
    assert((drawDeltaKeyboard<4,12>(unicode,100,"Name",keys,false,false,true,0,0,5)));
    strictUtf8Tail=false;assert(unicode.length()==80);
    char numeric[3][4][2]={};numeric[0][0][0]='1';numeric[0][0][1]='1';
    assert((drawDeltaKeyboard<3,4>("1",4,"Value",numeric,false,false,true,0,0,5)));
    available=false;assert((!drawDeltaKeyboard<4,12>("",4,"",keys,false,false,true,0,0,5)));
#endif
}
'''
        zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='delta-inputs-') as temp:
            cpp = Path(temp) / 'inputs.cpp'
            cpp.write_text(source, encoding='utf-8')
            for delta in (False, True):
                exe = Path(temp) / ('delta.exe' if delta else 'legacy.exe')
                flags = ['-DFLUX_DELTA_BUILD=1'] if delta else []
                result = subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG', *flags,
                                         '-I', str(ROOT / 'include'), str(cpp), '-o', str(exe)], capture_output=True, text=True)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)


if __name__ == '__main__':
    unittest.main()
