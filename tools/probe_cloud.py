#!/usr/bin/env python3
"""Probe La Marzocco lion cloud — verify auth, dump dashboard JSON, sniff WS."""
import base64, hashlib, json, os, sys, time, uuid
import requests
from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric.ec import ECDSA, SECP256R1, generate_private_key

B64 = lambda b: base64.b64encode(b).decode("ascii")
URL = "https://lion.lamarzocco.io/api/customer-app"

def request_proof(base: str, secret32: bytes) -> str:
    work = bytearray(secret32)
    for bv in base.encode("utf-8"):
        idx = bv % 32
        sa = work[(idx + 1) % 32] & 7
        x = bv ^ work[idx]
        work[idx] = ((x << sa) | (x >> (8 - sa))) & 0xFF
    return B64(hashlib.sha256(work).digest())

def main():
    user = os.environ["LM_USER"]; pwd = os.environ["LM_PASS"]; sn = os.environ.get("LM_SN", "LM017912")
    inst_id = str(uuid.uuid4()).lower()
    priv = generate_private_key(SECP256R1())
    pub_der = priv.public_key().public_bytes(serialization.Encoding.DER, serialization.PublicFormat.SubjectPublicKeyInfo)
    inst_hash_b64 = B64(hashlib.sha256(inst_id.encode()).digest())
    secret = hashlib.sha256(f"{inst_id}.{B64(pub_der)}.{inst_hash_b64}".encode()).digest()
    base_string = f"{inst_id}.{B64(hashlib.sha256(pub_der).digest())}"

    def extra_headers():
        nonce = str(uuid.uuid4()).lower()
        ts = str(int(time.time() * 1000))
        pi = f"{inst_id}.{nonce}.{ts}"
        proof = request_proof(pi, secret)
        sig = priv.sign(f"{pi}.{proof}".encode(), ECDSA(hashes.SHA256()))
        return {"X-App-Installation-Id": inst_id, "X-Timestamp": ts, "X-Nonce": nonce, "X-Request-Signature": B64(sig)}

    # 1. register
    r = requests.post(f"{URL}/auth/init",
                      headers={"X-App-Installation-Id": inst_id,
                               "X-Request-Proof": request_proof(base_string, secret)},
                      json={"pk": B64(pub_der)}, timeout=15)
    print(f"[init]   {r.status_code} {r.text[:200]}")

    # 2. signin
    r = requests.post(f"{URL}/auth/signin", headers=extra_headers(),
                      json={"username": user, "password": pwd}, timeout=15)
    print(f"[signin] {r.status_code}")
    if r.status_code != 200:
        print(r.text[:500]); sys.exit(1)
    tok = r.json(); access = tok["accessToken"] if "accessToken" in tok else tok.get("access_token")
    print(f"  keys: {list(tok.keys())}")

    auth = lambda: {**extra_headers(), "Authorization": f"Bearer {access}"}

    # 3. things
    r = requests.get(f"{URL}/things", headers=auth(), timeout=15)
    print(f"[things] {r.status_code}")
    print(json.dumps(r.json(), indent=2)[:2000])

    # 4. dashboard
    r = requests.get(f"{URL}/things/{sn}/dashboard", headers=auth(), timeout=15)
    print(f"[dashboard] {r.status_code}")
    open("dashboard.json", "w").write(json.dumps(r.json(), indent=2))
    print(json.dumps(r.json(), indent=2)[:4000])

    # 5. settings
    r = requests.get(f"{URL}/things/{sn}/settings", headers=auth(), timeout=15)
    print(f"[settings] {r.status_code}")
    open("settings.json", "w").write(json.dumps(r.json(), indent=2))
    print(json.dumps(r.json(), indent=2)[:2000])

    # 6. STOMP WS — listen briefly
    try:
        import websocket
        ws = websocket.create_connection("wss://lion.lamarzocco.io/ws/connect",
                                         header=[f"{k}: {v}" for k, v in extra_headers().items()],
                                         timeout=15)
        ws.send(f"CONNECT\nhost:lion.lamarzocco.io\naccept-version:1.2,1.1,1.0\nheart-beat:0,0\nAuthorization:Bearer {access}\n\n\x00")
        print(f"[ws CONNECTED] {ws.recv()[:300]}")
        ws.send(f"SUBSCRIBE\ndestination:/ws/sn/{sn}/dashboard\nack:auto\nid:{uuid.uuid4()}\ncontent-length:0\n\n\x00")
        ws.settimeout(8)
        for _ in range(3):
            try:
                m = ws.recv()
                print(f"[ws msg] {m[:2000]}")
            except Exception as e:
                print(f"[ws] timeout/err: {e}"); break
        ws.close()
    except Exception as e:
        print(f"[ws] {e}")

if __name__ == "__main__":
    main()
