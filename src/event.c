#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../include/event.h"
#include "../include/crypto.h"

// Parse a JSON event into an event_t structure
event_t *event_from_json(json_t *json) {
    if (!json_is_object(json)) {
        return NULL;
    }
    
    event_t *event = calloc(1, sizeof(event_t));
    if (!event) {
        return NULL;
    }
    
    // Extract the basic fields
    json_t *id_json = json_object_get(json, "id");
    json_t *pubkey_json = json_object_get(json, "pubkey");
    json_t *created_at_json = json_object_get(json, "created_at");
    json_t *kind_json = json_object_get(json, "kind");
    json_t *content_json = json_object_get(json, "content");
    json_t *sig_json = json_object_get(json, "sig");
    
    // Validate each field
    if (!json_is_string(id_json) || !json_is_string(pubkey_json) ||
        !json_is_integer(created_at_json) || !json_is_integer(kind_json) ||
        !json_is_string(content_json) || !json_is_string(sig_json)) {
        event_free(event);
        return NULL;
    }
    
    // Copy ID
    const char *id = json_string_value(id_json);
    if (strlen(id) != 64) {
        event_free(event);
        return NULL;
    }
    strcpy(event->id, id);
    
    // Copy pubkey
    const char *pubkey = json_string_value(pubkey_json);
    if (strlen(pubkey) != 64) {
        event_free(event);
        return NULL;
    }
    strcpy(event->pubkey, pubkey);
    
    // Copy created_at
    event->created_at = json_integer_value(created_at_json);
    
    // Copy kind
    int kind = json_integer_value(kind_json);
    if (kind < 0 || kind > 65535) {
        event_free(event);
        return NULL;
    }
    event->kind = (uint16_t)kind;
    
    // Copy content
    const char *content = json_string_value(content_json);
    event->content = strdup(content);
    if (!event->content) {
        event_free(event);
        return NULL;
    }
    
    // Copy signature
    const char *sig = json_string_value(sig_json);
    if (strlen(sig) != 128) {
        event_free(event);
        return NULL;
    }
    strcpy(event->sig, sig);
    
    // Process tags
    json_t *tags_json = json_object_get(json, "tags");
    if (!json_is_array(tags_json)) {
        event_free(event);
        return NULL;
    }
    
    size_t tags_count = json_array_size(tags_json);
    event->tags = calloc(tags_count, sizeof(event_tag_t));
    if (!event->tags && tags_count > 0) {
        event_free(event);
        return NULL;
    }
    
    for (size_t i = 0; i < tags_count; i++) {
        json_t *tag_json = json_array_get(tags_json, i);
        if (!json_is_array(tag_json)) {
            event_free(event);
            return NULL;
        }
        
        size_t tag_count = json_array_size(tag_json);
        if (tag_count == 0) {
            continue;  // Skip empty tags
        }
        
        // Allocate memory for the tag values
        event->tags[event->tags_count].values = calloc(tag_count, sizeof(char *));
        if (!event->tags[event->tags_count].values) {
            event_free(event);
            return NULL;
        }
        
        // Copy each tag value
        size_t valid_values = 0;
        for (size_t j = 0; j < tag_count; j++) {
            json_t *value_json = json_array_get(tag_json, j);
            if (!json_is_string(value_json)) {
                continue;  // Skip non-string values
            }
            
            const char *value = json_string_value(value_json);
            event->tags[event->tags_count].values[valid_values] = strdup(value);
            if (!event->tags[event->tags_count].values[valid_values]) {
                event_free(event);
                return NULL;
            }
            valid_values++;
        }
        
        event->tags[event->tags_count].count = valid_values;
        event->tags_count++;
    }
    
    return event;
}

// Convert an event_t structure to JSON
json_t *event_to_json(const event_t *event) {
    if (!event) {
        return NULL;
    }
    
    json_t *json = json_object();
    if (!json) {
        return NULL;
    }
    
    // Add basic fields
    json_object_set_new(json, "id", json_string(event->id));
    json_object_set_new(json, "pubkey", json_string(event->pubkey));
    json_object_set_new(json, "created_at", json_integer(event->created_at));
    json_object_set_new(json, "kind", json_integer(event->kind));
    json_object_set_new(json, "content", json_string(event->content));
    json_object_set_new(json, "sig", json_string(event->sig));
    
    // Add tags
    json_t *tags_json = json_array();
    if (!tags_json) {
        json_decref(json);
        return NULL;
    }
    
    for (size_t i = 0; i < event->tags_count; i++) {
        json_t *tag_json = json_array();
        if (!tag_json) {
            json_decref(tags_json);
            json_decref(json);
            return NULL;
        }
        
        for (size_t j = 0; j < event->tags[i].count; j++) {
            json_array_append_new(tag_json, json_string(event->tags[i].values[j]));
        }
        
        json_array_append_new(tags_json, tag_json);
    }
    
    json_object_set_new(json, "tags", tags_json);
    
    return json;
}

// Free an event_t structure
void event_free(event_t *event) {
    if (event) {
        // Free content
        free(event->content);
        
        // Free tags
        for (size_t i = 0; i < event->tags_count; i++) {
            for (size_t j = 0; j < event->tags[i].count; j++) {
                free(event->tags[i].values[j]);
            }
            free(event->tags[i].values);
        }
        free(event->tags);
        
        // Free the event itself
        free(event);
    }
}

// Serialize an event for ID calculation
char *event_serialize_for_id(const event_t *event) {
    if (!event) {
        return NULL;
    }
    
    // Create the JSON array for serialization
    json_t *serialize_array = json_array();
    if (!serialize_array) {
        return NULL;
    }
    
    // Add the elements in the required order
    json_array_append_new(serialize_array, json_integer(0));
    json_array_append_new(serialize_array, json_string(event->pubkey));
    json_array_append_new(serialize_array, json_integer(event->created_at));
    json_array_append_new(serialize_array, json_integer(event->kind));
    
    // Add tags
    json_t *tags_array = json_array();
    for (size_t i = 0; i < event->tags_count; i++) {
        json_t *tag_array = json_array();
        for (size_t j = 0; j < event->tags[i].count; j++) {
            json_array_append_new(tag_array, json_string(event->tags[i].values[j]));
        }
        json_array_append_new(tags_array, tag_array);
    }
    json_array_append_new(serialize_array, tags_array);
    
    // Add content
    json_array_append_new(serialize_array, json_string(event->content));
    
    // Dump the JSON to a string without formatting
    char *serialized = json_dumps(serialize_array, JSON_COMPACT);
    json_decref(serialize_array);
    
    return serialized;
}

// Calculate the ID of an event (SHA-256 hash of serialized event data)
bool event_calculate_id(event_t *event) {
    if (!event) {
        return false;
    }
    
    // Serialize the event for ID calculation
    char *serialized = event_serialize_for_id(event);
    if (!serialized) {
        return false;
    }
    
    // Calculate SHA-256 hash
    bool result = crypto_sha256(serialized, event->id);
    
    // Clean up
    free(serialized);
    
    return result;
}

// Verify the signature of an event
bool event_verify_signature(const event_t *event) {
    if (!event) {
        return false;
    }
    
    // The signature is over the event ID
    return crypto_verify_signature(event->sig, event->pubkey, event->id);
}

// Check if an event is valid (correct format, ID, and signature)
bool event_is_valid(const event_t *event) {
    if (!event) {
        return false;
    }
    
    // Verify the event ID
    char calculated_id[65];
    char *serialized = event_serialize_for_id(event);
    if (!serialized) {
        return false;
    }
    
    bool id_valid = crypto_sha256(serialized, calculated_id);
    free(serialized);
    
    if (!id_valid || strcmp(calculated_id, event->id) != 0) {
        return false;
    }
    
    // Verify the signature
    return event_verify_signature(event);
}

// Check if an event matches a filter
bool event_matches_filter(const event_t *event, json_t *filter) {
    if (!event || !filter || !json_is_object(filter)) {
        return false;
    }
    
    // Check ids filter
    json_t *ids = json_object_get(filter, "ids");
    if (json_is_array(ids) && json_array_size(ids) > 0) {
        bool match = false;
        size_t size = json_array_size(ids);
        
        for (size_t i = 0; i < size; i++) {
            json_t *id = json_array_get(ids, i);
            if (json_is_string(id) && strcmp(json_string_value(id), event->id) == 0) {
                match = true;
                break;
            }
        }
        
        if (!match) {
            return false;
        }
    }
    
    // Check authors filter
    json_t *authors = json_object_get(filter, "authors");
    if (json_is_array(authors) && json_array_size(authors) > 0) {
        bool match = false;
        size_t size = json_array_size(authors);
        
        for (size_t i = 0; i < size; i++) {
            json_t *author = json_array_get(authors, i);
            if (json_is_string(author) && strcmp(json_string_value(author), event->pubkey) == 0) {
                match = true;
                break;
            }
        }
        
        if (!match) {
            return false;
        }
    }
    
    // Check kinds filter
    json_t *kinds = json_object_get(filter, "kinds");
    if (json_is_array(kinds) && json_array_size(kinds) > 0) {
        bool match = false;
        size_t size = json_array_size(kinds);
        
        for (size_t i = 0; i < size; i++) {
            json_t *kind = json_array_get(kinds, i);
            if (json_is_integer(kind) && json_integer_value(kind) == event->kind) {
                match = true;
                break;
            }
        }
        
        if (!match) {
            return false;
        }
    }
    
    // Check since filter
    json_t *since = json_object_get(filter, "since");
    if (json_is_integer(since) && event->created_at < json_integer_value(since)) {
        return false;
    }
    
    // Check until filter
    json_t *until = json_object_get(filter, "until");
    if (json_is_integer(until) && event->created_at > json_integer_value(until)) {
        return false;
    }
    
    // Check tag filters (e.g., #e, #p, etc.)
    const char *key;
    json_t *value;
    
    json_object_foreach(filter, key, value) {
        // Check if this is a tag filter
        if (key[0] == '#' && strlen(key) >= 2 && json_is_array(value)) {
            char tag_name = key[1];
            bool match = false;
            
            // Look for this tag in the event
            for (size_t i = 0; i < event->tags_count; i++) {
                if (event->tags[i].count > 0 && 
                    event->tags[i].values[0][0] == tag_name && 
                    event->tags[i].values[0][1] == '\0') {
                    
                    // Found a matching tag, now check if any of the values match
                    size_t filter_values_size = json_array_size(value);
                    
                    for (size_t j = 0; j < filter_values_size; j++) {
                        json_t *filter_value = json_array_get(value, j);
                        
                        if (json_is_string(filter_value) && 
                            event->tags[i].count >= 2 && 
                            strcmp(json_string_value(filter_value), event->tags[i].values[1]) == 0) {
                            match = true;
                            break;
                        }
                    }
                    
                    if (match) {
                        break;
                    }
                }
            }
            
            if (!match && json_array_size(value) > 0) {
                return false;
            }
        }
    }
    
    // If we get here, all filter criteria match
    return true;
}

// Create an event_t structure with the given parameters
event_t *event_create(const char *pubkey, uint64_t created_at, uint16_t kind, 
                    json_t *tags_json, const char *content, const char *sig) {
    if (!pubkey || !content) {
        return NULL;
    }
    
    event_t *event = calloc(1, sizeof(event_t));
    if (!event) {
        return NULL;
    }
    
    // Copy pubkey
    if (strlen(pubkey) != 64) {
        event_free(event);
        return NULL;
    }
    strcpy(event->pubkey, pubkey);
    
    // Set created_at
    event->created_at = created_at;
    
    // Set kind
    event->kind = kind;
    
    // Copy content
    event->content = strdup(content);
    if (!event->content) {
        event_free(event);
        return NULL;
    }
    
    // Process tags
    if (tags_json && json_is_array(tags_json)) {
        size_t tags_count = json_array_size(tags_json);
        event->tags = calloc(tags_count, sizeof(event_tag_t));
        if (!event->tags && tags_count > 0) {
            event_free(event);
            return NULL;
        }
        
        for (size_t i = 0; i < tags_count; i++) {
            json_t *tag_json = json_array_get(tags_json, i);
            if (!json_is_array(tag_json)) {
                continue;
            }
            
            size_t tag_count = json_array_size(tag_json);
            if (tag_count == 0) {
                continue;
            }
            
            // Allocate memory for the tag values
            event->tags[event->tags_count].values = calloc(tag_count, sizeof(char *));
            if (!event->tags[event->tags_count].values) {
                event_free(event);
                return NULL;
            }
            
            // Copy each tag value
            size_t valid_values = 0;
            for (size_t j = 0; j < tag_count; j++) {
                json_t *value_json = json_array_get(tag_json, j);
                if (!json_is_string(value_json)) {
                    continue;
                }
                
                const char *value = json_string_value(value_json);
                event->tags[event->tags_count].values[valid_values] = strdup(value);
                if (!event->tags[event->tags_count].values[valid_values]) {
                    event_free(event);
                    return NULL;
                }
                valid_values++;
            }
            
            event->tags[event->tags_count].count = valid_values;
            event->tags_count++;
        }
    }
    
    // Calculate the event ID
    if (!event_calculate_id(event)) {
        event_free(event);
        return NULL;
    }
    
    // Copy signature if provided
    if (sig) {
        if (strlen(sig) != 128) {
            event_free(event);
            return NULL;
        }
        strcpy(event->sig, sig);
    }
    
    return event;
} 