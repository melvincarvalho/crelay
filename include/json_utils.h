#ifndef CRELAY_JSON_UTILS_H
#define CRELAY_JSON_UTILS_H

#include <jansson.h>
#include <stdbool.h>
#include <stdint.h>

// Extract a string from a JSON object with validation
bool json_get_string(json_t *obj, const char *key, char *dest, size_t dest_size);

// Extract an unsigned 64-bit integer from a JSON object
bool json_get_uint64(json_t *obj, const char *key, uint64_t *dest);

// Extract an unsigned 16-bit integer from a JSON object
bool json_get_uint16(json_t *obj, const char *key, uint16_t *dest);

// Check if a JSON object contains a specific field
bool json_has_field(json_t *obj, const char *key);

// Create a JSON array for an EVENT message response
json_t *json_create_event_response(const char *subscription_id, json_t *event);

// Create a JSON array for an OK message response
json_t *json_create_ok_response(const char *event_id, bool success, const char *message);

// Create a JSON array for an EOSE message response
json_t *json_create_eose_response(const char *subscription_id);

// Create a JSON array for a CLOSED message response
json_t *json_create_closed_response(const char *subscription_id, const char *message);

// Create a JSON array for a NOTICE message response
json_t *json_create_notice_response(const char *message);

#endif // CRELAY_JSON_UTILS_H 