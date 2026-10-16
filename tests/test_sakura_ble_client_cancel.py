"""Exercise the actual BLE worker wait/cancellation with an inert scheduled backend."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]

class BleClientCancellationTests(unittest.TestCase):
    def test_owned_worker_lifetime_and_cancellation(self):
        source = (ROOT / 'src/modules/ble/ble_common.cpp').read_text(encoding='utf-8')
        start = source.index('bool bleClientOperation(')
        end = source.index('\nvoid stopBLEStack()', start)
        code = r'''
#include <atomic>
#include <cstdint>
#include <cassert>
#include <functional>
#include <string>
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
#define BLE_HS_CONN_HANDLE_NONE 65535
bool EscPress=false;
bool check(bool&v){bool r=v;v=false;return r;}
void clearInputState(){EscPress=false;}
int ticks=0,finishAt=3,backAt=-1,retryAt=-1,disconnectAt=-1,errors=0,workers=0;
bool taskWorks=true,showedCancel=false,disconnectWorks=true;
bool gapTailDone=true;int gapTailAt=-1,barrierRuns=0,eventDeinits=0,holdBarrierUntil=0;
uint32_t millis(){return ticks*10;}
struct ble_npl_event{void(*fn)(ble_npl_event*);void*arg;};
struct ble_npl_eventq{} hostQueue;
ble_npl_event*barrier=nullptr;
ble_npl_eventq*nimble_port_get_dflt_eventq(){return &hostQueue;}
void ble_npl_event_init(ble_npl_event*ev,void(*fn)(ble_npl_event*),void*arg){ev->fn=fn;ev->arg=arg;}
void*ble_npl_event_get_arg(ble_npl_event*ev){return ev->arg;}
void ble_npl_eventq_put(ble_npl_eventq*q,ble_npl_event*ev){assert(q==&hostQueue&&!barrier);barrier=ev;}
void ble_npl_event_deinit(ble_npl_event*){assert(!barrier&&gapTailDone&&barrierRuns==eventDeinits+1);++eventDeinits;}
void(*worker)(void*)=nullptr;void*argument=nullptr;
void displayTextLine(const char*s){if(std::string(s)=="Cancelling...")showedCancel=true;}
void displayError(const char*){++errors;}
struct NimBLEClient{
 struct Config{bool deleteOnDisconnect=false,deleteOnConnectFail=false;}config;
 int handle=BLE_HS_CONN_HANDLE_NONE,disconnects=0;
 Config getConfig(){return config;}
 int getConnHandle(){return handle;}
 bool disconnect(){++disconnects;if(!disconnectWorks)return false;disconnectAt=ticks+2;gapTailAt=ticks+5;gapTailDone=false;return true;}
}*current=nullptr;
int xTaskCreate(void(*fn)(void*),const char*,int,void*ctx,int,void*){
 if(!taskWorks)return 0;assert(!worker);worker=fn;argument=ctx;++workers;return pdPASS;
}
void vTaskDelete(void*){--workers;}
void vTaskDelay(int){
 assert(++ticks<250);
 if(ticks==backAt||ticks==retryAt)EscPress=true;
 if(ticks==retryAt)disconnectWorks=true;
 if(ticks==disconnectAt)current->handle=BLE_HS_CONN_HANDLE_NONE;
 // GAP publishes NONE before its final client access. Host queue cannot advance yet.
 if(ticks==gapTailAt)gapTailDone=true;
 if(barrier&&gapTailDone&&ticks>=holdBarrierUntil){auto ev=barrier;barrier=nullptr;++barrierRuns;ev->fn(ev);}
 if(worker&&ticks>=finishAt){auto fn=worker;worker=nullptr;fn(argument);}
}
''' + source[start:end] + r'''
void reset(NimBLEClient&c){
 ticks=0;finishAt=3;backAt=retryAt=disconnectAt=-1;errors=workers=0;
 taskWorks=disconnectWorks=true;showedCancel=false;EscPress=false;worker=nullptr;current=&c;
 gapTailDone=true;gapTailAt=-1;barrierRuns=eventDeinits=holdBarrierUntil=0;barrier=nullptr;
}
int main(){
 {NimBLEClient c;reset(c);bool cancel=false;EscPress=true;int calls=0;
  assert(!bleClientOperation(&c,[&](){++calls;return true;},"Connecting...",cancel));
  assert(cancel&&calls==0&&workers==0);}
 {NimBLEClient c;reset(c);bool cancel=false;
  assert(bleClientOperation(&c,[]{return true;},"Connecting...",cancel));assert(!cancel&&workers==0&&ticks>=3);}
 // A connect succeeds after Back: caller stays pinned until owned disconnect completes.
 {NimBLEClient c;reset(c);bool cancel=false;backAt=1;
  assert(!bleClientOperation(&c,[&](){c.handle=7;return true;},"Connecting...",cancel));
  assert(cancel&&showedCancel&&c.handle==BLE_HS_CONN_HANDLE_NONE&&ticks>=5&&workers==0);}
 // Worker DONE + handle NONE is not enough: GAP still accesses the client afterward.
 {NimBLEClient c;reset(c);bool cancel=false;c.handle=7;finishAt=1;backAt=1;
  assert(!bleClientOperation(&c,[]{return true;},"Connecting...",cancel));
  assert(cancel&&gapTailDone&&ticks>=gapTailAt&&barrierRuns==1&&eventDeinits==1);}
 // Back arrives while the first host fence is queued, after worker success.
 {NimBLEClient c;reset(c);bool cancel=false;finishAt=1;backAt=2;
  assert(!bleClientOperation(&c,[&](){c.handle=7;return true;},"Connecting...",cancel));
  assert(cancel&&showedCancel&&gapTailDone&&c.handle==BLE_HS_CONN_HANDLE_NONE);
  assert(barrierRuns==2&&eventDeinits==2);}
 // A busy host keeps ownership past one second, acknowledges Back, and reports once.
 {NimBLEClient c;reset(c);bool cancel=false;finishAt=1;backAt=5;holdBarrierUntil=130;
  assert(!bleClientOperation(&c,[&](){c.handle=7;return true;},"Connecting...",cancel));
  assert(cancel&&showedCancel&&errors==1&&ticks>=135&&gapTailDone);
  assert(c.handle==BLE_HS_CONN_HANDLE_NONE&&barrierRuns==2&&eventDeinits==2);}
 // Normal finalization uses the same NONE/tail fence without creating a task.
 {NimBLEClient c;reset(c);bool cancel=false;c.handle=7;
  assert(bleClientDisconnect(&c,cancel));
  assert(!cancel&&workers==0&&worker==nullptr&&gapTailDone&&ticks>=gapTailAt);
  assert(barrierRuns==1&&eventDeinits==1);}
 // Already-canceled cleanup still joins the owned client and host without a worker.
 {NimBLEClient c;reset(c);bool cancel=true;c.handle=7;
  assert(bleClientDisconnect(&c,cancel));assert(cancel&&workers==0&&gapTailDone);}
 // Discovery worker can outlive the disconnect event; its context still stays alive.
 {NimBLEClient c;reset(c);bool cancel=false;c.handle=7;backAt=1;finishAt=6;
  assert(!bleClientOperation(&c,[]{return false;},"Discovering...",cancel));
  assert(cancel&&ticks>=6&&workers==0&&c.handle==BLE_HS_CONN_HANDLE_NONE);}
 // Failed disconnect retains ownership; a new Back retries without deleting the worker.
 {NimBLEClient c;reset(c);bool cancel=false;c.handle=7;backAt=1;retryAt=4;disconnectWorks=false;
  assert(!bleClientOperation(&c,[]{return true;},"Discovering...",cancel));
  assert(errors==1&&c.disconnects==2&&ticks>=6&&workers==0);}
 {NimBLEClient c;reset(c);bool cancel=false;c.config.deleteOnDisconnect=true;
  assert(!bleClientOperation(&c,[]{assert(false);return true;},"Discovering...",cancel));assert(errors==1&&workers==0);}
 {NimBLEClient c;reset(c);bool cancel=false;taskWorks=false;
  assert(!bleClientOperation(&c,[]{assert(false);return true;},"Connecting...",cancel));assert(errors==1&&workers==0);}
 for(int i=0;i<100;++i){NimBLEClient c;reset(c);bool cancel=false;backAt=1;
  assert(!bleClientOperation(&c,[]{return false;},"Connecting...",cancel));assert(workers==0&&cancel);}
}
'''
        with tempfile.TemporaryDirectory(prefix='sakura-ble-client-') as directory:
            exe = Path(directory) / 'client.exe'
            zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
            built = subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG', '-x', 'c++', '-', '-o', str(exe)], input=code, text=True, capture_output=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

    def test_strategy_cancellation_cannot_start_the_next_attempt(self):
        source = (ROOT / 'src/modules/ble/BLE_Suite.cpp').read_text(encoding='utf-8')
        start = source.index('NimBLEClient *attemptConnectionWithStrategies(')
        end = source.index('// HID Exploit Engine', start)
        production = source[start:end]
        code = r"""
#include <cassert>
#include <functional>
#include <string>
using String=std::string;
#define TFT_WHITE 0
#define TFT_YELLOW 0
#define TFT_ORANGE 0
#define TFT_CYAN 0
#define ESP_PWR_LVL_P9 0
bool g_bleOperationCancelled=false;
int cancelAt=1,attempts=0,delays=0,resets=0,inits=0,cleanups=0,deletes=0,cancelCleanup=0,cancelReset=0;
int cancelDelays=0,cancelResets=0,cancelInits=0,cancelCleanups=0;
void markCancel(){g_bleOperationCancelled=true;cancelDelays=delays;cancelResets=resets;cancelInits=inits;cancelCleanups=cleanups;}
void attempt(){if(++attempts==cancelAt)markCancel();}
void delay(int){++delays;}
void showAttackProgress(const char*,int){}
struct NimBLEAddress{std::string toString(){return "fake";}};
struct NimBLEClient{void setConnectTimeout(int){}void setConnectionParams(int,int,int,int){}bool connect(NimBLEAddress,bool){return false;}}client;
struct NimBLEDevice{
 static void setPower(int){}static void setSecurityAuth(bool,bool,bool){}
 static NimBLEClient*createClient(){return &client;}
 static void deleteClient(NimBLEClient*){++deletes;}
 static void init(std::string){++inits;}
};
struct BLEStateManager{
 static void registerClient(NimBLEClient*){}static void unregisterClient(NimBLEClient*){}
 static bool releaseClient(NimBLEClient*c){NimBLEDevice::deleteClient(c);return true;}
 static bool deinitBLE(bool){++resets;if(resets==cancelReset)markCancel();return true;}
};
struct BLEAttackManager{
 void prepareForConnection(){}
 bool connectToDevice(NimBLEAddress,NimBLEClient**,bool){attempt();return false;}
 void cleanupAfterAttack(){++cleanups;if(cleanups==cancelCleanup)markCancel();}
};
bool bleClientOperation(NimBLEClient*,const std::function<bool()>&,const char*,bool&){attempt();return false;}
struct DeviceInfo{std::string address;bool hasHFP=false;};
struct {int size(){return 0;}bool getDeviceInfo(int,DeviceInfo&){return false;}}scannerData;
struct HFPExploitEngine{bool establishHFPConnection(NimBLEAddress){assert(false);return false;}};
""" + production + r"""
int main(){for(cancelAt=1;cancelAt<=3;++cancelAt){
 g_bleOperationCancelled=false;attempts=delays=resets=inits=cleanups=deletes=0;
 String method;assert(attemptConnectionWithStrategies({},method)==nullptr);
 assert(g_bleOperationCancelled&&attempts==cancelAt);
 assert(delays==cancelDelays&&resets==cancelResets&&inits==cancelInits&&cleanups==cancelCleanups);
 if(cancelAt>1)assert(deletes>0);
}
 // Back can be acquired by cleanup itself, after an ordinary failed connection.
 for(int at=1;at<=2;++at){
  cancelAt=99;cancelCleanup=at;cancelReset=0;g_bleOperationCancelled=false;
  attempts=delays=resets=inits=cleanups=deletes=0;
  String method;assert(attemptConnectionWithStrategies({},method)==nullptr);
  assert(g_bleOperationCancelled&&cleanups==at);
  assert(delays==cancelDelays&&resets==cancelResets&&inits==cancelInits&&cleanups==cancelCleanups);
 }
 // A post-cleanup deinit can also acquire Back before the next raw SDK init.
 cancelCleanup=0;cancelReset=1;cancelAt=99;g_bleOperationCancelled=false;
 attempts=delays=resets=inits=cleanups=deletes=0;
 String method;assert(attemptConnectionWithStrategies({},method)==nullptr);
 assert(g_bleOperationCancelled&&resets==1);
 assert(delays==cancelDelays&&resets==cancelResets&&inits==cancelInits&&cleanups==cancelCleanups);
}
"""
        with tempfile.TemporaryDirectory(prefix='sakura-ble-strategy-') as directory:
            exe = Path(directory) / 'strategy.exe'
            zig = next((ROOT / '.tools/zig').rglob('zig.exe'))
            built = subprocess.run([str(zig), 'c++', '-std=c++11', '-O2', '-UNDEBUG', '-x', 'c++', '-', '-o', str(exe)], input=code, text=True, capture_output=True, timeout=120)
            self.assertEqual(built.returncode, 0, built.stdout + built.stderr)
            ran = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(ran.returncode, 0, ran.stdout + ran.stderr)

if __name__ == '__main__':
    unittest.main()
