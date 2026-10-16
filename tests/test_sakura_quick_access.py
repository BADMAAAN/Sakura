"""Run the persisted favorite IDs and RAM-only MRU used by firmware."""
from pathlib import Path
import subprocess
import tempfile
import unittest
import re

ROOT = Path(__file__).resolve().parents[1]


class QuickAccessTests(unittest.TestCase):
    def test_actual_resolver_navigation_and_migration(self):
        source = (ROOT / 'src/core/sakura_navigation.cpp').read_text(encoding='utf-8')
        source = re.sub(r'^#include.*$', '', source, flags=re.M)
        fixture = (ROOT / 'tests/sakura_navigation_integration.cpp').read_text(encoding='utf-8')
        code = fixture.replace('/* PRODUCTION_NAVIGATION */', source)
        compiler = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-navigation-') as temp:
            executable = Path(temp) / 'navigation.exe'
            result = subprocess.run([str(compiler), 'c++', '-std=c++11', '-O2', '-UNDEBUG',
                                     '-I', str(ROOT / 'include'), '-I', str(ROOT / 'src'),
                                     '-x', 'c++', '-', '-o', str(executable)], input=code, text=True,
                                    capture_output=True)
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
            subprocess.run([str(executable)], check=True)

    def test_actual_history_allowlist_ignores_private_values(self):
        source = (ROOT / 'src/core/sakura_navigation.cpp').read_text(encoding='utf-8')
        from test_delta_runtime import function
        sections = source[source.index('const char *const sections[]'):source.index('const char *const screens[]')]
        code = r'''
#include <cassert>
#include <cstring>
#include <delta/quick_access.h>
delta::QuickAccess quick; int loads=0;
void load(){++loads;}
'''
        code += sections + function(source, 'void sakuraRecordMenu(')
        code += r'''
int main(){
 sakuraRecordMenu("Main Menu","WiFi");assert(quick.recentCount==1&&quick.recent[0]==delta::QuickAction(1,0));
 sakuraRecordMenu("Diagnostics","Buttons");assert(quick.recent[0]==delta::QuickAction(2,1));
 for(int i=0;i<100;++i){
  sakuraRecordMenu("SSH","private-user@private-host");
  sakuraRecordMenu("QRCodes","bitcoin:private-payment-data");
  sakuraRecordMenu("Results","private SSID or address");
 }
 assert(quick.recentCount==2); // No dynamic labels or contents cross the allowlist.
 quick.history=false;sakuraRecordMenu("Main Menu","Config");assert(quick.recentCount==2);
}
'''
        compiler = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-history-') as temp:
            executable = Path(temp) / 'history.exe'
            subprocess.run([str(compiler), 'c++', '-std=c++11', '-O2', '-UNDEBUG', '-I',
                            str(ROOT / 'include'), '-x', 'c++', '-', '-o', str(executable)],
                           input=code, text=True, check=True)
            subprocess.run([str(executable)], check=True)

    def test_order_ids_decode_and_private_history_boundary(self):
        code = r'''
#include <cassert>
#include <delta/quick_access.h>
int main() {
 using namespace delta;
 QuickAccess state; uint8_t bytes[QuickBytes];
 assert(state.pin({1,14},true)&&state.pin({2,1},true)&&state.pin({3,73},true)&&state.pin({4,9},true));
 assert(state.pin({3,73},true)&&state.count==4);
 auto size=encodeQuickAccess(state,bytes); QuickAccess decoded;
 assert(decodeQuickAccess(bytes,size,decoded)&&decoded.count==4);
 assert(decoded.pins[2]==QuickAction(3,73)); // Stable identity survives renames/menu reorder.
 assert(decoded.pin({2,1},false)&&decoded.count==3&&decoded.pins[1]==QuickAction(3,73));
 for(unsigned i=0;i<8;++i)decoded.record({1,i});
 assert(decoded.recentCount==5&&decoded.recent[0]==QuickAction(1,7)&&decoded.recent[4]==QuickAction(1,3));
 decoded.record({1,5});assert(decoded.recentCount==5&&decoded.recent[0]==QuickAction(1,5));
 decoded.record({3,73});decoded.record({4,9});assert(decoded.recent[0]==QuickAction(1,5));
 decoded.history=false;decoded.record({2,1});assert(decoded.recent[0]==QuickAction(1,5));
 size=encodeQuickAccess(decoded,bytes);QuickAccess restarted;
 assert(decodeQuickAccess(bytes,size,restarted)&&!restarted.history&&restarted.recentCount==0);
 const auto before=restarted;
 bytes[2]=2;assert(!decodeQuickAccess(bytes,size,restarted)&&restarted.count==before.count);
 bytes[2]=1;bytes[3]=25;assert(!decodeQuickAccess(bytes,size,restarted));
 bytes[3]=1;assert(!decodeQuickAccess(bytes,size,restarted));
 bytes[3]=3;bytes[6]=99;assert(!decodeQuickAccess(bytes,size,restarted));
 QuickAccess full;for(unsigned i=1;i<=QuickAccess::Capacity;++i)assert(full.pin({3,i},true));
 assert(!full.pin({3,100},true)&&full.count==QuickAccess::Capacity);
 assert(validQuickAction({1,0})&&!validQuickAction({1,15})&&!validQuickAction({2,5})&&!validQuickAction({3,0})&&!validQuickAction({4,0}));
}
'''
        compiler = next((ROOT / '.tools/zig').rglob('zig.exe'))
        with tempfile.TemporaryDirectory(prefix='sakura-quick-') as temp:
            executable = Path(temp) / 'quick.exe'
            subprocess.run([str(compiler), 'c++', '-std=c++11', '-O2', '-UNDEBUG', '-I',
                            str(ROOT / 'include'), '-x', 'c++', '-', '-o', str(executable)],
                           input=code, text=True, check=True)
            subprocess.run([str(executable)], check=True)


if __name__ == '__main__':
    unittest.main()
