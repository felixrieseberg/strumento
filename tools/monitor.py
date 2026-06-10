#!/usr/bin/env python3
import sys, time, serial
p = serial.Serial("/dev/cu.usbserial-5B1F0085301", 115200, timeout=0.2)
# pulse reset (RTS=EN on Core2)
p.setDTR(False); p.setRTS(True); time.sleep(0.1); p.setRTS(False); time.sleep(0.1)
deadline = time.time() + float(sys.argv[1] if len(sys.argv)>1 else 30)
while time.time() < deadline:
    d = p.read(4096)
    if d:
        sys.stdout.write(d.decode("utf-8","replace")); sys.stdout.flush()
