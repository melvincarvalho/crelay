#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <openssl/sha.h>
#include <secp256k1.h>
#include <secp256k1_schnorrsig.h>
#include "../include/crypto.h"

// Convert a hex string to binary
bool crypto_hex_decode(const char *hex, unsigned char *bin, size_t expected_bin_len) {
    if (!hex || !bin) {
        return false;
    }
    
    size_t hex_len = strlen(hex);
    if (hex_len != expected_bin_len * 2) {
        return false;
    }
    
    for (size_t i = 0; i < expected_bin_len; i++) {
        char byte_str[3] = {hex[i*2], hex[i*2+1], '\0'};
        char *endptr;
        bin[i] = (unsigned char)strtol(byte_str, &endptr, 16);
        if (*endptr != '\0') {
            return false;
        }
    }
    
    return true;
}

// Convert binary data to a hex string
void crypto_hex_encode(const unsigned char *bin, size_t bin_len, char *hex) {
    if (!bin || !hex) {
        return;
    }
    
    for (size_t i = 0; i < bin_len; i++) {
        sprintf(hex + (i * 2), "%02x", bin[i]);
    }
    hex[bin_len * 2] = '\0';
}

// Calculate SHA-256 hash of a string
bool crypto_sha256(const char *input, char *output_hex) {
    if (!input || !output_hex) {
        return false;
    }
    
    unsigned char hash[SHA256_DIGEST_LENGTH];
    SHA256_CTX sha256;
    
    if (SHA256_Init(&sha256) != 1) {
        return false;
    }
    
    if (SHA256_Update(&sha256, input, strlen(input)) != 1) {
        return false;
    }
    
    if (SHA256_Final(hash, &sha256) != 1) {
        return false;
    }
    
    crypto_hex_encode(hash, SHA256_DIGEST_LENGTH, output_hex);
    
    return true;
}

// Verify a Schnorr signature according to the Nostr protocol
bool crypto_verify_signature(const char *signature_hex, const char *pubkey_hex, const char *message_hash_hex) {
    if (!signature_hex || !pubkey_hex || !message_hash_hex) {
        return false;
    }
    
    // Decode the hex strings
    unsigned char signature[64];
    unsigned char pubkey[32];
    unsigned char message_hash[32];
    
    if (!crypto_hex_decode(signature_hex, signature, 64) ||
        !crypto_hex_decode(pubkey_hex, pubkey, 32) ||
        !crypto_hex_decode(message_hash_hex, message_hash, 32)) {
        return false;
    }
    
    // Create and initialize a secp256k1 context
    secp256k1_context *ctx = secp256k1_context_create(SECP256K1_CONTEXT_VERIFY);
    if (!ctx) {
        return false;
    }
    
    // Parse the public key
    secp256k1_xonly_pubkey xonly_pubkey;
    if (!secp256k1_xonly_pubkey_parse(ctx, &xonly_pubkey, pubkey)) {
        secp256k1_context_destroy(ctx);
        return false;
    }
    
    // Verify the signature
    int result = secp256k1_schnorrsig_verify(ctx, signature, message_hash, 32, &xonly_pubkey);
    
    // Clean up
    secp256k1_context_destroy(ctx);
    
    return result == 1;
} 