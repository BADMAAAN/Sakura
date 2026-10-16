"""Encode the user-selected Sakura reference and original DELTA icons for the native display."""
from pathlib import Path
import math
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "assets/delta"
OUT.mkdir(parents=True, exist_ok=True)
S = 4
# User-selected reference, kept in the source package so builds do not depend on Desktop.
# Resize/crop only; keep its tree, moon, lake and original detail rather than recreating them.
import argparse
from PIL import ImageOps
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--header-only", action="store_true", help="Leave preview PNGs unchanged")
args = parser.parse_args()
im = ImageOps.fit(Image.open(OUT / "cyber-reference.png").convert("RGB"), (320, 170),
                  method=Image.Resampling.LANCZOS, centering=(0.5, 0.5))
pix = im.load()
for y in range(170):
    for x in range(320):
        # Quiet control area: maintain detail on the left and enough negative space on the right.
        t = max(0.0, min(1.0, (x - 176) / 112))
        gain = .92 * (1 - .73 * (t * t * (3 - 2 * t)))
        r, g, b = pix[x, y]
        pix[x, y] = (round(r * gain), round(g * gain), round(b * gain))
if not args.header_only:
    im.save(OUT / "dark-sakura-source.png")
indexed = im.quantize(colors=64, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE)
pal = indexed.getpalette()[:192]

def rgb565(rgb):
    r,g,b=rgb; return ((int(r)>>3)<<11)|((int(g)>>2)<<5)|(int(b)>>3)
palettes=[]
for theme in range(3):
    colors=[]
    for i in range(64):
        r,g,b=pal[i*3:i*3+3]
        if theme==1:
            # A warmer rose grade, retaining deep black negative space.
            r,g,b=(min(255,r*1.04+2),g*.86,min(255,b*.87+1))
        elif theme==2:
            lum=.3*r+.59*g+.11*b; r,g,b=lum,lum,lum
        colors.append(rgb565((r,g,b)))
    palettes.append(colors)
for t,name in enumerate(("dark-sakura","sakura-dawn","monochrome")):
    show=Image.new("RGB",(320,170)); out=[]
    for v in indexed.tobytes():
        c=palettes[t][v]; out.append((((c>>11)&31)*255//31,((c>>5)&63)*255//63,(c&31)*255//31))
    show.putdata(out)
    if not args.header_only: show.save(OUT/(name+".png"))
# A coherent 40px outline icon family. Raster alpha is shared by host and MCU.
icons=[]
for n in range(13):
    ico=Image.new("L",(40*S,40*S)); q=ImageDraw.Draw(ico)
    def ln(p,w=2.1): q.line([(int(x*S),int(y*S)) for x,y in p],fill=255,width=round(w*S),joint="curve")
    def arc(box,start,end,w=2.1): q.arc(tuple(round(v*S) for v in box),start,end,fill=255,width=round(w*S))
    def rect(box,rad=2): q.rounded_rectangle(tuple(round(v*S) for v in box),radius=rad*S,outline=255,width=round(2.1*S))
    def circle(x,y,r,fill=False): q.ellipse(((x-r)*S,(y-r)*S,(x+r)*S,(y+r)*S),fill=255 if fill else None,outline=255,width=round(2.1*S))
    if n==0:
        for rad in (8,15,22): arc((20-rad,29-rad,20+rad,29+rad),225,315)
        circle(20,29,1.7,True)
    elif n==1: ln([(14,12),(27,25),(20,33),(20,7),(27,15),(14,28)])
    elif n==2:
        ln([(20,17),(20,32),(13,32),(27,32)]); circle(20,15,2,True)
        for rad in (7,13):
            arc((20-rad,15-rad,20+rad,15+rad),135,225);arc((20-rad,15-rad,20+rad,15+rad),315,405)
    elif n==3:
        ln([(8,30),(8,20),(14,20),(14,12),(20,12),(20,25),(26,25),(26,18),(32,18),(32,9)])
        circle(8,30,1.5,True);circle(32,9,1.5,True)
    elif n==4:
        rect((14,17,26,34),3);circle(20,22,1.5,True)
        for rad in (5,10):arc((20-rad,16-rad,20+rad,16+rad),225,315)
    elif n==5:
        rect((8,10,32,29)); ln([(13,29),(13,24),(27,24),(27,29)])
        for x in (14,18,22,26):ln([(x,10),(x,16)],1.5)
    elif n==6:
        rect((7,10,33,31),3);circle(13,16,1.5,True)
        arc((12,13,28,29),295,425);arc((16,17,24,25),295,425)
    elif n==7: ln([(6,31),(6,12),(16,12),(19,16),(34,16),(31,31),(6,31)]);ln([(7,20),(32,20)])
    elif n==8:
        ln([(15,11),(7,20),(15,29)]);ln([(25,11),(33,20),(25,29)]);ln([(23,7),(18,33)])
    elif n==9: circle(20,20,14);ln([(20,11),(20,20),(27,24)])
    elif n==10:
        ln([(10,31),(25,16),(30,16),(33,10),(28,12),(25,8),(24,14),(9,29)],2.1)
    elif n==11:
        points=[]
        for k in range(48):
            a=k*math.tau/48;r=15 if k%6 in(1,2,3,4) else 11.5;points.append((20+r*math.cos(a),20+r*math.sin(a)))
        ln(points+[points[0]],2);circle(20,20,5)
    else:
        points=[]
        for k in range(10):
            a=-math.pi/2+k*math.pi/5;r=16 if k%2==0 else 7
            points.append((20+r*math.cos(a),20+r*math.sin(a)))
        ln(points+[points[0]])
    ico=ico.resize((40,40),Image.Resampling.LANCZOS)
    icons.append(bytes((v+8)//17 for v in ico.tobytes()))
    if not args.header_only: ico.save(OUT/("icon-%02d.png"%n))
font=ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf",11)
glyphs=[]
advances=[]
for ch in range(32,127):
    glyph=Image.new("L",(12*S,13*S));g=ImageDraw.Draw(glyph)
    bigfont=ImageFont.truetype("C:/Windows/Fonts/segoeui.ttf",11*S)
    g.text((0,-2*S),chr(ch),font=bigfont,fill=255)
    glyph=glyph.resize((12,13),Image.Resampling.LANCZOS)
    glyphs.append(bytes((v+8)//17 for v in glyph.tobytes()))
    advances.append(math.ceil(font.getlength(chr(ch))))
def packed(data):return bytes((data[i]<<4)|data[i+1] for i in range(0,len(data),2))
def array(name,data,kind="uint8_t"):
    per=24 if kind=="uint8_t" else 12
    declaration = "const %s %s[%d]" % (kind, name, len(data))
    return "#if defined(ARDUINO) && defined(FLUX_DELTA_BUILD)\nextern\n#else\nstatic\n#endif\n" + declaration + "\n#if !defined(ARDUINO) || !defined(FLUX_DELTA_BUILD) || defined(DELTA_ARTWORK_IMPLEMENTATION)\n= {\n" + "\n".join("    "+",".join(str(v) for v in data[i:i+per])+"," for i in range(0,len(data),per)) + "\n}\n#endif\n;\n"
header="#pragma once\n#include <stdint.h>\n// Generated by tools/delta_preview/generate_artwork.py; user reference background and original DELTA icons.\nnamespace delta { namespace artwork {\n"
header+=array("background",indexed.tobytes())
for i,p in enumerate(palettes):header+=array("palette%d"%i,p,"uint16_t")
header+=array("iconAlpha",packed(b"".join(icons)))
header+=array("glyphAlpha",packed(b"".join(glyphs)))
header+=array("glyphAdvance",advances)
header+="} }\n"
(ROOT/"include/delta").mkdir(parents=True,exist_ok=True)
(ROOT/"include/delta/artwork.h").write_text(header,encoding="utf-8")
print("Artwork generated: 54400 indexed pixels + 10400 icon alpha + 7410 glyph alpha bytes")
