"""Small host watchdog; restarts supervisor after crashes, never mounts secrets."""
import argparse, subprocess, sys, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]; STOP=ROOT/'ai'/'state'/'stop.requested'
def main():
    ap=argparse.ArgumentParser(); ap.add_argument('--max-restarts',type=int,default=0); ap.add_argument('--probe-restart'); a=ap.parse_args(); restarts=0
    STOP.unlink(missing_ok=True)
    while not STOP.exists() and (not a.max_restarts or restarts<a.max_restarts):
        command=[sys.executable,str(ROOT/'ai'/'supervisor.py')]
        command += ['--watchdog-probe',a.probe_restart] if a.probe_restart else ['--daemon']
        p=subprocess.Popen(command,cwd=ROOT)
        code=p.wait(); restarts+=1
        if STOP.exists(): break
        time.sleep(min(300,2**min(restarts,8)))
    return 0
if __name__=='__main__': raise SystemExit(main())
