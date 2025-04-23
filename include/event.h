#ifndef CRELAY_EVENT_H
#define CRELAY_EVENT_H

#include <jansson.h>
#include <stdbool.h>
#include <stdint.h>

// Maximum length for subscription IDs
#define MAX_SUBSCRIPTION_ID_LENGTH 64

// Structure for a tag in an event
typedef struct {
    char **values;
    size_t count;
} event_tag_t;

// Structure for an event
typedef struct {
    char id[65];            // 32-bytes hex-encoded SHA-256 hash
    char pubkey[65];        // 32-bytes hex-encoded public key
    uint64_t created_at;    // Unix timestamp in seconds
    uint16_t kind;          // Event kind (0-65535)
    event_tag_t *tags;      // Array of tags
    size_t tags_count;      // Number of tags
    char *content;          // Content string
    char sig[129];          // 64-bytes hex-encoded signature
} event_t;

// Parse a JSON event into an event_t structure
event_t *event_from_json(json_t *json);

// Convert an event_t structure to JSON
json_t *event_to_json(const event_t *event);

// Free an event_t structure
void event_free(event_t *event);

// Verify the signature of an event
bool event_verify_signature(const event_t *event);

// Calculate the ID of an event (SHA-256 hash of serialized event data)
bool event_calculate_id(event_t *event);

// Check if an event is valid (correct format, ID, and signature)
bool event_is_valid(const event_t *event);

// Check if an event matches a filter
bool event_matches_filter(const event_t *event, json_t *filter);

// Serialize an event for ID calculation
char *event_serialize_for_id(const event_t *event);

// Create an event_t structure with the given parameters
event_t *event_create(const char *pubkey, uint64_t created_at, uint16_t kind, 
                    json_t *tags_json, const char *content, const char *sig);

#endif // CRELAY_EVENT_H 