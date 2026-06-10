#include "lm_crypto.h"
#include "storage.h"

#include <mbedtls/base64.h>
#include <mbedtls/ecdsa.h>
#include <mbedtls/ecp.h>
#include <mbedtls/pk.h>
#include <mbedtls/sha256.h>
#include <mbedtls/ctr_drbg.h>
#include <mbedtls/entropy.h>
#include <esp_random.h>
#include <sys/time.h>

namespace lmcrypto {

static mbedtls_pk_context       g_pk;
static mbedtls_entropy_context  g_ent;
static mbedtls_ctr_drbg_context g_rng;
static uint8_t  g_pubDer[128];  static size_t g_pubDerLen = 0;
static uint8_t  g_secret[32];
static String   g_pubB64;

// ── small helpers ────────────────────────────────────────────────────────────
String b64(const uint8_t* p, size_t n) {
  size_t outLen = 0;
  size_t cap = 4 * ((n + 2) / 3) + 4;
  String out; out.reserve(cap);
  uint8_t* buf = (uint8_t*)malloc(cap);
  mbedtls_base64_encode(buf, cap, &outLen, p, n);
  out.concat((const char*)buf, outLen);
  free(buf);
  return out;
}

void sha256(const uint8_t* p, size_t n, uint8_t out[32]) {
  mbedtls_sha256(p, n, out, 0);
}

String uuid4() {
  uint8_t r[16]; esp_fill_random(r, 16);
  r[6] = (r[6] & 0x0F) | 0x40;       // version 4
  r[8] = (r[8] & 0x3F) | 0x80;       // variant 1
  char s[37];
  snprintf(s, sizeof s,
    "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
    r[0],r[1],r[2],r[3], r[4],r[5], r[6],r[7],
    r[8],r[9], r[10],r[11],r[12],r[13],r[14],r[15]);
  return String(s);
}

String lmProof(const String& base, const uint8_t secret[32]) {
  uint8_t work[32]; memcpy(work, secret, 32);
  for (size_t i = 0; i < base.length(); ++i) {
    uint8_t bv  = (uint8_t)base[i];
    uint8_t idx = bv % 32;
    uint8_t sa  = work[(idx + 1) % 32] & 7;
    uint8_t x   = bv ^ work[idx];
    // python: (x << sa) | (x >> (8 - sa))  — when sa==0, x>>8 == 0, so this is identity.
    work[idx]   = sa ? (uint8_t)((x << sa) | (x >> (8 - sa))) : x;
  }
  uint8_t h[32]; sha256(work, 32, h);
  return b64(h, 32);
}

// ── key material ─────────────────────────────────────────────────────────────
static bool loadOrGenKey() {
  mbedtls_pk_init(&g_pk);
  mbedtls_entropy_init(&g_ent);
  mbedtls_ctr_drbg_init(&g_rng);
  if (mbedtls_ctr_drbg_seed(&g_rng, mbedtls_entropy_func, &g_ent,
                            (const uint8_t*)"lm-crema", 8) != 0) return false;
  if (mbedtls_pk_setup(&g_pk, mbedtls_pk_info_from_type(MBEDTLS_PK_ECKEY)) != 0)
    return false;
  mbedtls_ecp_keypair* kp = mbedtls_pk_ec(g_pk);
  if (mbedtls_ecp_group_load(&kp->grp, MBEDTLS_ECP_DP_SECP256R1) != 0) return false;

  if (settings.ecPrivValid) {
    if (mbedtls_mpi_read_binary(&kp->d, settings.ecPriv, 32) != 0) return false;
    if (mbedtls_ecp_mul(&kp->grp, &kp->Q, &kp->d, &kp->grp.G,
                        mbedtls_ctr_drbg_random, &g_rng) != 0) return false;
  } else {
    if (mbedtls_ecp_gen_keypair(&kp->grp, &kp->d, &kp->Q,
                                mbedtls_ctr_drbg_random, &g_rng) != 0) return false;
    mbedtls_mpi_write_binary(&kp->d, settings.ecPriv, 32);
    settings.ecPrivValid = true;
  }
  // DER SubjectPublicKeyInfo (writes at END of buffer)
  uint8_t tmp[160];
  int n = mbedtls_pk_write_pubkey_der(&g_pk, tmp, sizeof tmp);
  if (n <= 0) return false;
  g_pubDerLen = (size_t)n;
  memcpy(g_pubDer, tmp + sizeof tmp - n, n);
  g_pubB64 = b64(g_pubDer, g_pubDerLen);
  return true;
}

static void deriveSecret() {
  uint8_t idHash[32];
  sha256((const uint8_t*)settings.instId.c_str(), settings.instId.length(), idHash);
  String triple = settings.instId + "." + g_pubB64 + "." + b64(idHash, 32);
  sha256((const uint8_t*)triple.c_str(), triple.length(), g_secret);
}

bool init() {
  bool fresh = false;
  if (settings.instId.isEmpty()) { settings.instId = uuid4(); fresh = true; }
  if (!loadOrGenKey()) return false;
  if (!settings.ecPrivValid) return false;
  if (fresh || !settings.ecPrivValid) settings.save();
  else if (fresh == false && settings.ecPrivValid) {
    // first-boot path may have flipped ecPrivValid above
  }
  // ensure persisted if anything was generated
  settings.save();
  deriveSecret();
  return true;
}

const String& installationId() { return settings.instId; }
const String& publicKeyB64()   { return g_pubB64; }

String initProof() {
  uint8_t pubHash[32]; sha256(g_pubDer, g_pubDerLen, pubHash);
  String base = settings.instId + "." + b64(pubHash, 32);
  return lmProof(base, g_secret);
}

// ── per-request signing ──────────────────────────────────────────────────────
SignedHeaders sign() {
  SignedHeaders h;
  h.instId = settings.instId;
  h.nonce  = uuid4();
  struct timeval tv; gettimeofday(&tv, nullptr);
  uint64_t ms = (uint64_t)tv.tv_sec * 1000ULL + tv.tv_usec / 1000;
  char tbuf[24]; snprintf(tbuf, sizeof tbuf, "%llu", (unsigned long long)ms);
  h.timestamp = tbuf;

  String pi    = h.instId + "." + h.nonce + "." + h.timestamp;
  String proof = lmProof(pi, g_secret);
  String data  = pi + "." + proof;

  uint8_t digest[32];
  sha256((const uint8_t*)data.c_str(), data.length(), digest);

  uint8_t sig[MBEDTLS_ECDSA_MAX_LEN]; size_t sigLen = 0;
  mbedtls_ecdsa_write_signature(mbedtls_pk_ec(g_pk), MBEDTLS_MD_SHA256,
                                digest, 32, sig, &sigLen,
                                mbedtls_ctr_drbg_random, &g_rng);
  h.signature = b64(sig, sigLen);
  return h;
}

String SignedHeaders::asHeaderBlock() const {
  String s;
  s.reserve(256);
  s += "X-App-Installation-Id: "; s += instId;    s += "\r\n";
  s += "X-Timestamp: ";           s += timestamp; s += "\r\n";
  s += "X-Nonce: ";               s += nonce;     s += "\r\n";
  s += "X-Request-Signature: ";   s += signature;
  return s;
}

}  // namespace lmcrypto
