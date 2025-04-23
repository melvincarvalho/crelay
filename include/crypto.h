#ifndef CRELAY_CRYPTO_H
#define CRELAY_CRYPTO_H

#include <stdbool.h>
#include <stdint.h>

// Calculate SHA-256 hash of a string
bool crypto_sha256(const char *input, char *output_hex);

// Verify a Schnorr signature according to the Nostr protocol
bool crypto_verify_signature(const char *signature_hex, const char *pubkey_hex, const char *message_hash_hex);

// Hex encoding/decoding utilities
bool crypto_hex_decode(const char *hex, unsigned char *bin, size_t expected_bin_len);
void crypto_hex_encode(const unsigned char *bin, size_t bin_len, char *hex);

#endif // CRELAY_CRYPTO_H 