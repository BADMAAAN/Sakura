"""Inert checks against the actual Suite client ownership/finalization functions."""
from pathlib import Path
import subprocess
import tempfile
import unittest
ROOT=Path(__file__).resolve().parents[1]

def function(source, signature):
    start=source.index(signature); opening=source.index('{',start); depth=1; end=opening+1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[start:end]

class SuiteOwnershipTests(unittest.TestCase):
    def test_registry_owned_teardown_foreign_survival_and_unsafe_rapid_failure(self):
        source=(ROOT/'src/modules/ble/BLE_Suite.cpp').read_text(encoding='utf-8')
        production='\n'.join(function(source,sig) for sig in (
            'bool BLEStateManager::deinitBLE(', 'bool BLEStateManager::ownsClient(',
            'void BLEStateManager::registerClient(', 'void BLEStateManager::unregisterClient(',
            'bool BLEStateManager::releaseClient(', 'bool BLEStateManager::cleanupAllClients(',
            'bool FastPairExploitEngine::executeRapidConnection('))
        code=r'''
#include <algorithm>
#include <cassert>
#include <string>
#include <vector>
using String=std::string;
bool g_bleOperationCancelled=false;int errors=0,globalDeinits=0;
void displayError(const char*){++errors;}void showErrorMessage(const char*){++errors;}
struct NimBLEAddress{};struct NimBLERemoteCharacteristic{};
struct NimBLEClient{bool connected=true,tail=false,foreign=false,deleted=false,canRelease=true,deleteWorks=true;};
std::vector<std::string>order;
struct BLEStateManager{
 static bool bleInitialized;static String currentDeviceName;
 static std::vector<NimBLEClient*>activeClients;
 static bool deinitBLE(bool);static bool ownsClient(NimBLEClient*);
 static void registerClient(NimBLEClient*);static void unregisterClient(NimBLEClient*);
 static bool releaseClient(NimBLEClient*);static bool cleanupAllClients();
};
bool BLEStateManager::bleInitialized=true;String BLEStateManager::currentDeviceName="Suite";
std::vector<NimBLEClient*>BLEStateManager::activeClients;
struct NimBLEDevice{
 static void deinit(bool){++globalDeinits;}
 static bool deleteClient(NimBLEClient*c){
  assert(c&&!c->foreign&&!c->connected&&c->tail&&!c->deleted);
  assert(!BLEStateManager::ownsClient(c));if(!c->deleteWorks)return false;c->deleted=true;order.push_back("delete");return true;
 }
};
bool bleClientDisconnect(NimBLEClient*c,bool&){
 if(!c->canRelease)return false;
 assert(!c->foreign);order.push_back("disconnect");c->connected=false;order.push_back("NONE");
 c->tail=true;order.push_back("host-tail");return true;
}
struct FastPairExploitEngine{bool executeRapidConnection(NimBLEAddress,NimBLERemoteCharacteristic*);};
''' + production + r'''
int main(){
 NimBLEClient owned,foreign;foreign.foreign=true;
 BLEStateManager::registerClient(&owned);BLEStateManager::registerClient(&owned);
 assert(BLEStateManager::activeClients.size()==1);
 // Remove all historical duplicates, not merely the first entry.
 BLEStateManager::activeClients.push_back(&owned);BLEStateManager::unregisterClient(&owned);
 assert(BLEStateManager::activeClients.empty());
 BLEStateManager::registerClient(&owned);assert(BLEStateManager::releaseClient(&owned));
 assert((order==std::vector<std::string>{"disconnect","NONE","host-tail","delete"}));
 assert(owned.deleted&&foreign.connected&&!foreign.deleted&&globalDeinits==0);
 assert(!BLEStateManager::releaseClient(&foreign));assert(foreign.connected&&!foreign.deleted);
 NimBLEClient second;BLEStateManager::registerClient(&second);BLEStateManager::deinitBLE(true);
 assert(second.deleted&&foreign.connected&&!foreign.deleted&&globalDeinits==0);
 // Refused ownership/lifetime release must retain the registry and object.
 NimBLEClient blocked;blocked.canRelease=false;BLEStateManager::registerClient(&blocked);
 BLEStateManager::bleInitialized=true;BLEStateManager::currentDeviceName="Retained";
 assert(!BLEStateManager::deinitBLE(true));
 assert(BLEStateManager::ownsClient(&blocked)&&!blocked.deleted&&g_bleOperationCancelled);
 assert(BLEStateManager::bleInitialized&&BLEStateManager::currentDeviceName=="Retained");
 blocked.canRelease=true;g_bleOperationCancelled=true;assert(BLEStateManager::cleanupAllClients());
 assert(blocked.deleted&&BLEStateManager::activeClients.empty()&&g_bleOperationCancelled);
 // Exceptional SDK deletion failure is never reported as success or inactive.
 NimBLEClient deletionFailed;deletionFailed.deleteWorks=false;BLEStateManager::registerClient(&deletionFailed);
 BLEStateManager::bleInitialized=true;BLEStateManager::currentDeviceName="Delete retained";
 g_bleOperationCancelled=false;assert(!BLEStateManager::deinitBLE(true));
 assert(g_bleOperationCancelled&&BLEStateManager::ownsClient(&deletionFailed)&&!deletionFailed.deleted);
 assert(BLEStateManager::bleInitialized&&BLEStateManager::currentDeviceName=="Delete retained");
 deletionFailed.deleteWorks=true;assert(BLEStateManager::deinitBLE(true));
 assert(deletionFailed.deleted&&!BLEStateManager::bleInitialized&&BLEStateManager::currentDeviceName.empty());
 // The unsafe legacy branch must fail before touching even an invalid characteristic.
 FastPairExploitEngine rapid;int before=errors;
 assert(!rapid.executeRapidConnection({},reinterpret_cast<NimBLERemoteCharacteristic*>(1)));
 assert(errors==before);g_bleOperationCancelled=false;
 assert(!rapid.executeRapidConnection({},reinterpret_cast<NimBLERemoteCharacteristic*>(1)));
 assert(errors==before+1);
 assert(foreign.connected&&!foreign.deleted&&globalDeinits==0);
}
'''
        with tempfile.TemporaryDirectory(prefix='sakura-suite-ownership-') as directory:
            exe=Path(directory)/'ownership.exe';zig=next((ROOT/'.tools/zig').rglob('zig.exe'))
            built=subprocess.run([str(zig),'c++','-std=c++11','-O2','-UNDEBUG','-x','c++','-','-o',str(exe)],input=code,text=True,capture_output=True,timeout=120)
            self.assertEqual(built.returncode,0,built.stdout+built.stderr)
            ran=subprocess.run([str(exe)],capture_output=True,text=True,timeout=10)
            self.assertEqual(ran.returncode,0,ran.stdout+ran.stderr)
if __name__=='__main__':unittest.main()
