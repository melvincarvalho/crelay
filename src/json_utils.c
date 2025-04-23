#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../include/json_utils.h"

// Extract a string from a JSON object with validation
bool json_get_string(json_t *obj, const char *key, char *dest, size_t dest_size) {
    if (!obj || !key || !dest || dest_size == 0) {
        return false;
    }
    
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_string(value)) {
        return false;
    }
    
    const char *str = json_string_value(value);
    if (strlen(str) >= dest_size) {
        return false;
    }
    
    strcpy(dest, str);
    return true;
}

// Extract an unsigned 64-bit integer from a JSON object
bool json_get_uint64(json_t *obj, const char *key, uint64_t *dest) {
    if (!obj || !key || !dest) {
        return false;
    }
    
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_integer(value)) {
        return false;
    }
    
    *dest = (uint64_t)json_integer_value(value);
    return true;
}

// Extract an unsigned 16-bit integer from a JSON object
bool json_get_uint16(json_t *obj, const char *key, uint16_t *dest) {
    if (!obj || !key || !dest) {
        return false;
    }
    
    json_t *value = json_object_get(obj, key);
    if (!value || !json_is_integer(value)) {
        return false;
    }
    
    json_int_t integer = json_integer_value(value);
    if (integer < 0 || integer > UINT16_MAX) {
        return false;
    }
    
    *dest = (uint16_t)integer;
    return true;
}

// Check if a JSON object contains a specific field
bool json_has_field(json_t *obj, const char *key) {
    if (!obj || !key) {
        return false;
    }
    
    json_t *value = json_object_get(obj, key);
    return value != NULL;
}

// Create a JSON array for an EVENT message response
json_t *json_create_event_response(const char *subscription_id, json_t *event) {
    if (!subscription_id || !event) {
        return NULL;
    }
    
    json_t *response = json_array();
    if (!response) {
        return NULL;
    }
    
    json_array_append_new(response, json_string("EVENT"));
    json_array_append_new(response, json_string(subscription_id));
    json_array_append(response, event);
    
    return response;
}

// Create a JSON array for an OK message response
json_t *json_create_ok_response(const char *event_id, bool success, const char *message) {
    if (!event_id) {
        return NULL;
    }
    
    json_t *response = json_array();
    if (!response) {
        return NULL;
    }
    
    json_array_append_new(response, json_string("OK"));
    json_array_append_new(response, json_string(event_id));
    json_array_append_new(response, json_boolean(success));
    json_array_append_new(response, json_string(message ? message : ""));
    
    return response;
}

// Create a JSON array for an EOSE message response
json_t *json_create_eose_response(const char *subscription_id) {
    if (!subscription_id) {
        return NULL;
    }
    
    json_t *response = json_array();
    if (!response) {
        return NULL;
    }
    
    json_array_append_new(response, json_string("EOSE"));
    json_array_append_new(response, json_string(subscription_id));
    
    return response;
}

// Create a JSON array for a CLOSED message response
json_t *json_create_closed_response(const char *subscription_id, const char *message) {
    if (!subscription_id) {
        return NULL;
    }
    
    json_t *response = json_array();
    if (!response) {
        return NULL;
    }
    
    json_array_append_new(response, json_string("CLOSED"));
    json_array_append_new(response, json_string(subscription_id));
    json_array_append_new(response, json_string(message ? message : ""));
    
    return response;
}

// Create a JSON array for a NOTICE message response
json_t *json_create_notice_response(const char *message) {
    if (!message) {
        return NULL;
    }
    
    json_t *response = json_array();
    if (!response) {
        return NULL;
    }
    
    json_array_append_new(response, json_string("NOTICE"));
    json_array_append_new(response, json_string(message));
    
    return response;
} 