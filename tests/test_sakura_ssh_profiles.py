"""Run extracted firmware SSH policy with inert clock, key and NVS doubles."""
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
WORKSPACE = next(p for p in Path(__file__).resolve().parents if (p / '.tools/zig').is_dir())


def function(source, signature):
    start = source.index(signature)
    opening = source.index('{', start)
    depth, end = 1, opening + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class SshProfilesTests(unittest.TestCase):
    def test_manager_releases_storage_before_child(self):
        profiles = (ROOT / 'src/modules/wifi/ssh_profiles.cpp').read_text(encoding='utf-8')
        source = r'''
#include <cassert>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#define MENU_TYPE_SUBMENU 1
using String = std::string;
bool returnToMenu=false;
int stores=0, loops=0, opened=0, edits=0;
constexpr unsigned MAX_PROFILES=12;
struct Profile { uint32_t id=0; String name; };
struct Store {
 unsigned count=1; uint32_t nextId=74; Profile entries[MAX_PROFILES];
 Store(){++stores;} Store(const Store &){++stores;} ~Store(){--stores;}
};
Store load(){return Store();}
struct SshProfileLink {uint32_t id; String name;};
std::vector<SshProfileLink> sshProfileLinks(){Store snapshot=load();return {{73,"saved"}};}
bool openSshProfile(uint32_t id){assert(stores==0&&id==73);++opened;return true;}
bool edit(Profile &){assert(stores==1);++edits;return false;}
bool save(const Store &){assert(false);return false;}
void displayError(const char *,bool){}
struct Option {String label;std::function<void()> operation;};
int loopOptions(std::vector<Option>&menu,int,const char*){
 assert(stores==0);
 if(++loops==1){menu[1].operation();return 1;}
 if(loops==2){menu[0].operation();return 0;}
 menu.back().operation();return int(menu.size())-1;
}
''' + function(profiles, 'void sshProfilesMenu() {') + r'''
int main(){sshProfilesMenu();assert(stores==0&&opened==1&&edits==1&&loops==3);}
'''
        with tempfile.TemporaryDirectory(prefix='sakura-ssh-stack-') as directory:
            exe = Path(directory) / 'manager.exe'
            zig = next((WORKSPACE / '.tools/zig').rglob('zig.exe'))
            subprocess.run([str(zig), 'c++', '-std=c++11', '-O1', '-UNDEBUG',
                            '-x', 'c++', '-', '-o', str(exe)], input=source, text=True, check=True)
            subprocess.run([str(exe)], check=True)

    def test_fake_backend(self):
        profiles = (ROOT / 'src/modules/wifi/ssh_profiles.cpp').read_text(encoding='utf-8')
        clients = (ROOT / 'src/modules/wifi/clients.cpp').read_text(encoding='utf-8')
        source = (ROOT / 'tests/sakura_ssh_profiles_test.cpp').read_text(encoding='utf-8')
        production = '\n'.join(function(profiles, signature) for signature in (
            'bool sshValidPort(', 'bool sshValidEndpoint('))
        production += '\n' + profiles[profiles.index('constexpr size_t MAX_PROFILES'):profiles.index('bool edit(')]
        production += '\n' + function(clients, 'void clearSshPassword(')
        production += '\n' + function(clients, 'template <typename Fn> bool sshStep(')
        production += '\n' + function(clients, 'bool verifySshServer(')
        production += '\n' + function(clients, 'bool sshFlushOutbound(')
        production += '\n' + function(clients, 'void closeSshTransport(')
        production += '\n' + function(clients, 'void finishSessionUi(')
        source = source.replace('/* PRODUCTION */', production)
        with tempfile.TemporaryDirectory(prefix='sakura-ssh-') as directory:
            exe = Path(directory) / 'ssh.exe'
            zig = next((WORKSPACE / '.tools/zig').rglob('zig.exe'))
            result = subprocess.run([str(zig), 'c++', '-std=c++11', '-O1', '-UNDEBUG',
                                     '-x', 'c++', '-', '-o', str(exe)], input=source,
                                    text=True, capture_output=True, cwd=ROOT)
            self.assertEqual(result.returncode, 0, result.stderr)
            result = subprocess.run([str(exe)], capture_output=True, text=True, timeout=10)
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_shared_path_and_secret_boundary(self):
        profiles = (ROOT / 'src/modules/wifi/ssh_profiles.cpp').read_text(encoding='utf-8')
        clients = (ROOT / 'src/modules/wifi/clients.cpp').read_text(encoding='utf-8')
        worker = function(clients, 'void sshWorkerRun(')
        self.assertLess(worker.index('verifySshServer('), worker.index('ssh_userauth_password('))
        self.assertIn('ssh_set_blocking(sshSession, 0)', worker)
        self.assertIn('SSH_OPTIONS_FD', worker)
        self.assertIn('sessionHost.c_str()', worker)
        self.assertNotIn('WiFi.hostByName', function(clients, 'void ssh_connect_profile('))
        self.assertIn('ssh_connect_profile(ssh_host, port, ssh_user)', function(clients, 'void ssh_setup('))
        self.assertNotIn('password', profiles[profiles.index('struct Profile {'):profiles.index('bool validProfile(')].lower())
        self.assertNotIn('FluxConfig', profiles)
        self.assertNotIn('toJson', profiles)
        finish = function(clients, 'void finishSessionUi(')
        self.assertIn('millis() - started < 3000', finish)
        self.assertIn('while (getClientTaskHandle() != nullptr)', finish)
        self.assertNotIn('vTaskDelete', finish)
        self.assertIn('closeSshTransport(sshSession, socketFd)', worker)
        self.assertNotIn('socketTransferred', worker)
        self.assertIn('if (pendingOutbound.isEmpty())', worker)
        self.assertIn('std::shared_ptr<SshDnsRequest>', function(clients, 'void sshDnsResult('))


if __name__ == '__main__':
    unittest.main()
