#!/usr/bin/env python3
import json, os, sys
sys.path.insert(0,os.path.dirname(__file__))
from probe_cloud import *  # reuse auth helpers
import requests, base64, hashlib, time, uuid
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric.ec import ECDSA, SECP256R1, generate_private_key

# inline minimal auth (probe_cloud has main() only)
B64=lambda b:base64.b64encode(b).decode()
URL="https://lion.lamarzocco.io/api/customer-app"
def proof(base,sec):
    w=bytearray(sec)
    for bv in base.encode():
        i=bv%32; sa=w[(i+1)%32]&7; x=bv^w[i]; w[i]=((x<<sa)|(x>>(8-sa)))&0xFF
    return B64(hashlib.sha256(w).digest())
iid=str(uuid.uuid4()).lower(); priv=generate_private_key(SECP256R1())
pub=priv.public_key().public_bytes(serialization.Encoding.DER,serialization.PublicFormat.SubjectPublicKeyInfo)
sec=hashlib.sha256(f"{iid}.{B64(pub)}.{B64(hashlib.sha256(iid.encode()).digest())}".encode()).digest()
def hdr():
    n=str(uuid.uuid4()).lower(); ts=str(int(time.time()*1000)); pi=f"{iid}.{n}.{ts}"
    sig=priv.sign(f"{pi}.{proof(pi,sec)}".encode(),ECDSA(hashes.SHA256()))
    return {"X-App-Installation-Id":iid,"X-Timestamp":ts,"X-Nonce":n,"X-Request-Signature":B64(sig)}
requests.post(f"{URL}/auth/init",headers={"X-App-Installation-Id":iid,"X-Request-Proof":proof(f"{iid}.{B64(hashlib.sha256(pub).digest())}",sec)},json={"pk":B64(pub)})
tok=requests.post(f"{URL}/auth/signin",headers=hdr(),json={"username":os.environ["LM_USER"],"password":os.environ["LM_PASS"]}).json()["accessToken"]
A=lambda:{**hdr(),"Authorization":f"Bearer {tok}"}
sn=os.environ.get("LM_SN","LM017912")

for path in [f"/things/{sn}/stats", f"/things/{sn}/scheduling",
             f"/things/{sn}/stats/CoffeeAndFlushCounter/1"]:
    r=requests.get(URL+path,headers=A(),timeout=15)
    print(f"\n=== {path} === {r.status_code}")
    print(json.dumps(r.json(),indent=2)[:3000])
