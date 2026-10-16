"""Windows preview of the *same C++ RGB565 renderer* used on the LilyGO.
Run: .venv Python tools/delta_preview/preview.py [--capture] [--port 8765]
All status values and section activations in this host preview are SIMULATED.
"""
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs
import argparse
import ctypes
import io
import json
import subprocess
import sys
import time
from PIL import Image

HERE=Path(__file__).resolve().parent
ROOT=HERE.parents[1]
DLL=HERE/"delta_host.dll"
def compiler():
    candidates=list((ROOT/".tools/zig").rglob("zig.exe"))
    if not candidates: raise SystemExit("Portable Zig not found under .tools/zig; see local build evidence.")
    return candidates[0]
def load():
    if not DLL.exists() or DLL.stat().st_mtime<max(p.stat().st_mtime for p in [HERE/"host.cpp",ROOT/"include/delta/core.h",ROOT/"include/delta/artwork.h"]):
        subprocess.run([str(compiler()),"c++","-std=c++11","-O3","-shared",str(HERE/"host.cpp"),"-o",str(DLL)],check=True,cwd=ROOT)
    lib=ctypes.CDLL(str(DLL))
    lib.delta_frame.argtypes=[ctypes.POINTER(ctypes.c_uint16)]+[ctypes.c_int]*9+[ctypes.c_uint32,ctypes.c_int,ctypes.c_uint32,ctypes.c_int,ctypes.c_int]
    lib.delta_benchmark.argtypes=[ctypes.c_int];lib.delta_benchmark.restype=ctypes.c_double
    return lib
LIB=None
def frame(**kwargs):
    defaults=dict(theme=0,petals=2,quality=1,reduced=0,glitch=0,icons=1,status=1,selected=0,previous=0,changed=0,direction=1,now=1000,ambient=0,speed=160)
    defaults.update(kwargs);v=defaults
    buf=(ctypes.c_uint16*(320*170))()
    LIB.delta_frame(buf,*[v[k] for k in defaults])
    return Image.frombytes("RGB",(320,170),bytes(buf),"raw","BGR;16")
def capture():
    out=ROOT/"assets/delta/preview";out.mkdir(parents=True,exist_ok=True)
    for theme,name in enumerate(("dark-sakura","sakura-dawn","monochrome")):
        im=frame(theme=theme);im.save(out/(name+"-320x170.png"));im.resize((1280,680),Image.Resampling.NEAREST).save(out/(name+"-4x.png"))
    for i in range(13):frame(selected=i,previous=i).save(out/("section-%02d.png"%i))
    frame(ambient=1).save(out/"ambient.png")
    sequence=[]
    for i in range(100):
        now=i*20;selected=(now//400)%5;changed=selected*400
        sequence.append(frame(now=now,selected=selected,previous=max(0,selected-1),changed=changed))
    sequence[0].save(out/"navigation.gif",save_all=True,append_images=sequence[1:],duration=20,loop=0,optimize=False)
    timing=LIB.delta_benchmark(1000)
    (out/"host-measurement.json").write_text(json.dumps({"renderer_mean_microseconds":round(timing,2),"iterations":1000,"platform":sys.platform,"hardware_fps":"NOT MEASURED","note":"CPU render only. Excludes LCD SPI, firmware work, image encoding and browser."},indent=2))
    print("CAPTURE",out,"CPU renderer us/frame",round(timing,2))
class Handler(BaseHTTPRequestHandler):
    def do_GET(self):
        uri=urlparse(self.path)
        if uri.path=="/frame":
            values={k:int(v[0]) for k,v in parse_qs(uri.query).items() if k in {"theme","petals","quality","reduced","glitch","icons","status","selected","previous","changed","direction","now","ambient","speed"}}
            values["theme"]=max(0,min(2,values.get("theme",0)))
            values["selected"]=values.get("selected",0)%13;values["previous"]=values.get("previous",0)%13
            im=frame(**values);stream=io.BytesIO();im.save(stream,format="PNG");body=stream.getvalue();kind="image/png"
        else:body=(HERE/"index.html").read_bytes();kind="text/html; charset=utf-8"
        self.send_response(200);self.send_header("Content-Type",kind);self.send_header("Cache-Control","no-store");self.send_header("Content-Length",str(len(body)));self.end_headers();self.wfile.write(body)
    def log_message(self,*args):pass
if __name__=="__main__":
    parser=argparse.ArgumentParser();parser.add_argument("--capture",action="store_true");parser.add_argument("--port",type=int,default=8765);args=parser.parse_args()
    LIB=load()
    if args.capture:capture()
    else:print("DELTA shared C++ preview: http://127.0.0.1:%d (SIMULATED hardware status)"%args.port,flush=True);ThreadingHTTPServer(("127.0.0.1",args.port),Handler).serve_forever()
