#pragma once
// La Marzocco "lion" cloud request-signing primitives.
//
// Flow (mirrors pylamarzocco/util/_authentication.py):
//   1. installation_id  : random lowercase UUID, persisted forever
//   2. P-256 keypair    : random, persisted forever (private scalar in NVS)
//   3. pub_der          : SubjectPublicKeyInfo DER of public key
//   4. secret[32]       : SHA256( id "." b64(pub_der) "." b64(SHA256(id)) )
//   5. /auth/init       : headers {X-App-Installation-Id, X-Request-Proof},
//                         body {"pk": b64(pub_der)}
//                         where proof = lmProof(id "." b64(SHA256(pub_der)), secret)
//   6. every other call : headers {X-App-Installation-Id, X-Timestamp, X-Nonce,
//                         X-Request-Signature} where signature =
//                         b64( ECDSA-SHA256-DER( id "." nonce "." ts "." proof ) )
//                         and proof = lmProof(id "." nonce "." ts, secret)

#include <Arduino.h>

namespace lmcrypto {

// Initialise from persisted material; generates keypair+id on first boot
// and writes them back via storage.save().
bool   init();

// Derived/cached material
const String& installationId();
const String& publicKeyB64();         // b64(DER SubjectPublicKeyInfo)
String        initProof();            // X-Request-Proof for /auth/init

// Per-request signed header block (4 headers, "\r\n"-joined, no trailing CRLF)
struct SignedHeaders {
  String instId, timestamp, nonce, signature;
  String asHeaderBlock() const;       // for WebSocket extra-headers
};
SignedHeaders sign();

// helpers (exposed for unit poking)
String uuid4();
String b64(const uint8_t* p, size_t n);
void   sha256(const uint8_t* p, size_t n, uint8_t out[32]);
String lmProof(const String& base, const uint8_t secret[32]);

}  // namespace lmcrypto
