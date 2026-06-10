#!/usr/bin/env python3
"""Grab framebuffer screenshots from the device into shots/*.png.

Opens the port once (which resets the Core2), waits for full boot + cloud
connect, then captures every screen.
"""
import sys, time, base64, os, re, serial
from PIL import Image
B64=re.compile(r"^[A-Za-z0-9+/]{4,}=*$")

PORT="/dev/cu.usbserial-5B1F0085301"; W,H=320,240
SCREENS={"home_on":"4","home_standby":"0","brew":"1","controls":"2","stats":"5","setup":"3"}
os.makedirs("shots",exist_ok=True)

p=serial.Serial(PORT,115200,timeout=0.5)
print("waiting for boot+cloud …")
deadline=time.time()+30; ready=False
while time.time()<deadline:
    line=p.readline().decode("utf-8","ignore")
    if line: sys.stdout.write("  "+line)
    if "STOMP CONNECTED" in line or "/dashboard ->" in line: ready=True
    if ready and time.time()>deadline-5: break
if not ready:
    # fall back to fixed wait — UI still renders without cloud
    print("  (no cloud marker, capturing anyway)")
time.sleep(2)

def grab(cmd):
    p.reset_input_buffer(); p.write((cmd+"s").encode()); p.flush()
    blob=bytearray(); t0=time.time()
    while time.time()-t0<40:
        chunk=p.read(8192)
        if chunk: blob+=chunk
        if b"###SHOT_END###" in blob: break
    s=blob.decode("ascii","ignore")
    raw=bytearray(W*H*2); got=0
    for m in re.finditer(r"#R(\d{3}):([A-Za-z0-9+/=]+)",s):
        y=int(m[1]); b=m[2]; b=b[:len(b)-len(b)%4]
        try: data=base64.b64decode(b)
        except Exception: continue
        if y<H and len(data)>=W*2:
            raw[y*W*2:(y+1)*W*2]=data[:W*2]; got+=1
    if got<H: print(f"  ({H-got} rows lost)")
    time.sleep(0.5); p.reset_input_buffer()
    return bytes(raw)

def save(raw,path):
    img=bytearray(W*H*3)
    for i in range(W*H):
        v=(raw[2*i]<<8)|raw[2*i+1]
        r=(v>>11)&31; g=(v>>5)&63; b=v&31
        img[3*i]=r*255//31; img[3*i+1]=g*255//63; img[3*i+2]=b*255//31
    Image.frombytes("RGB",(W,H),bytes(img)).save(path)
    print(f"  → {path}")

for name,cmd in SCREENS.items():
    save(grab(cmd),f"shots/{name}.png")
print("done")
import sys; sys.exit(0)

def save(raw,path):
    if len(raw)<W*H*2:
        print(f"  (short {len(raw)}/{W*H*2}, padding)"); raw=raw+b"\0"*(W*H*2-len(raw))
    img=bytearray(W*H*3)
    for i in range(W*H):
        v=(raw[2*i]<<8)|raw[2*i+1]          # M5GFX sprite buffer is byte-swapped
        r=(v>>11)&31; g=(v>>5)&63; b=v&31
        img[3*i]=r*255//31; img[3*i+1]=g*255//63; img[3*i+2]=b*255//31
    Image.frombytes("RGB",(W,H),bytes(img)).save(path)
    print(f"  → {path}")

for name,cmd in SCREENS.items():
    raw=grab(cmd)
    save(raw,f"shots/{name}.png")
print("done")
