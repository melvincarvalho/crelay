#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <jansson.h>
#include "../include/relay.h"
#include "../include/event.h"
#include "../include/database.h"
#include "../include/subscription.h"
#include "../include/json_utils.h"

// Maximum size of a message that can be sent/received
#define MAX_PAYLOAD_SIZE (512 * 1024)

// Per-session data
typedef struct {
    struct lws *wsi;
    unsigned char *rx_buffer;
    size_t rx_len;
    subscription_t **subscriptions;
    size_t subscriptions_count;
} client_session_t;

// Global relay state for use in libwebsockets callbacks
static relay_t *g_relay = NULL;

static int callback_nostr(struct lws *wsi, enum lws_callback_reasons reason,
                        void *user, void *in, size_t len) {
    client_session_t *session = (client_session_t *)user;
    
    switch (reason) {
        case LWS_CALLBACK_ESTABLISHED:
            // Initialize session data
            session->wsi = wsi;
            session->rx_buffer = malloc(MAX_PAYLOAD_SIZE);
            session->rx_len = 0;
            session->subscriptions = NULL;
            session->subscriptions_count = 0;
            
            // Send a welcome notice
            relay_send_notice(g_relay, wsi, "Welcome to Crelay Nostr Relay (NIP-01)");
            break;
            
        case LWS_CALLBACK_RECEIVE:
            // Append received data to buffer
            if (session->rx_len + len > MAX_PAYLOAD_SIZE) {
                relay_send_notice(g_relay, wsi, "Message too large");
                return -1;
            }
            
            memcpy(session->rx_buffer + session->rx_len, in, len);
            session->rx_len += len;
            
            // Check for complete message
            if (lws_is_final_fragment(wsi)) {
                // Null-terminate the message
                session->rx_buffer[session->rx_len] = '\0';
                
                // Parse the JSON message
                json_error_t error;
                json_t *message = json_loads((char *)session->rx_buffer, 0, &error);
                
                if (!message) {
                    relay_send_notice(g_relay, wsi, "Invalid JSON");
                } else {
                    // Process the message
                    if (json_is_array(message)) {
                        size_t size = json_array_size(message);
                        
                        if (size >= 1) {
                            json_t *command = json_array_get(message, 0);
                            
                            if (json_is_string(command)) {
                                const char *cmd = json_string_value(command);
                                
                                if (strcmp(cmd, "EVENT") == 0 && size >= 2) {
                                    // EVENT message
                                    json_t *event = json_array_get(message, 1);
                                    relay_process_event(g_relay, wsi, event);
                                } else if (strcmp(cmd, "REQ") == 0 && size >= 3) {
                                    // REQ message
                                    json_t *subscription_id_json = json_array_get(message, 1);
                                    
                                    if (json_is_string(subscription_id_json)) {
                                        const char *subscription_id = json_string_value(subscription_id_json);
                                        
                                        // Create array of filters from the message (everything after subscription_id)
                                        json_t *filters = json_array();
                                        for (size_t i = 2; i < size; i++) {
                                            json_array_append(filters, json_array_get(message, i));
                                        }
                                        
                                        // Process the REQ message
                                        relay_process_req(g_relay, wsi, subscription_id, filters);
                                        
                                        json_decref(filters);
                                    } else {
                                        relay_send_notice(g_relay, wsi, "Invalid subscription ID");
                                    }
                                } else if (strcmp(cmd, "CLOSE") == 0 && size >= 2) {
                                    // CLOSE message
                                    json_t *subscription_id_json = json_array_get(message, 1);
                                    
                                    if (json_is_string(subscription_id_json)) {
                                        const char *subscription_id = json_string_value(subscription_id_json);
                                        relay_process_close(g_relay, wsi, subscription_id);
                                    } else {
                                        relay_send_notice(g_relay, wsi, "Invalid subscription ID");
                                    }
                                } else {
                                    relay_send_notice(g_relay, wsi, "Unknown or malformed command");
                                }
                            } else {
                                relay_send_notice(g_relay, wsi, "Command must be a string");
                            }
                        } else {
                            relay_send_notice(g_relay, wsi, "Empty command array");
                        }
                    } else {
                        relay_send_notice(g_relay, wsi, "Message must be a JSON array");
                    }
                    
                    json_decref(message);
                }
                
                // Reset the buffer
                session->rx_len = 0;
            }
            break;
            
        case LWS_CALLBACK_CLOSED:
            // Free session resources
            if (session->rx_buffer) {
                free(session->rx_buffer);
                session->rx_buffer = NULL;
            }
            
            // Clean up subscriptions
            for (size_t i = 0; i < session->subscriptions_count; i++) {
                subscription_free(session->subscriptions[i]);
            }
            free(session->subscriptions);
            break;
            
        default:
            break;
    }
    
    return 0;
}

static struct lws_protocols protocols[] = {
    {
        "nostr",
        callback_nostr,
        sizeof(client_session_t),
        MAX_PAYLOAD_SIZE,
    },
    { NULL, NULL, 0, 0 } // terminator
};

relay_t *relay_init(const relay_config_t *config) {
    relay_t *relay = malloc(sizeof(relay_t));
    if (!relay) {
        return NULL;
    }
    
    // Store the configuration
    relay->config = config;
    
    // Initialize the database
    if (database_init(&relay->db, config->db_path) != 0) {
        free(relay);
        return NULL;
    }
    
    // Initialize libwebsockets
    struct lws_context_creation_info info;
    memset(&info, 0, sizeof(info));
    
    info.port = config->port;
    info.iface = config->bind_address;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;
    info.options = LWS_SERVER_OPTION_HTTP_HEADERS_SECURITY_BEST_PRACTICES_ENFORCE;
    
    relay->lws_context = lws_create_context(&info);
    if (!relay->lws_context) {
        database_close(relay->db);
        free(relay);
        return NULL;
    }
    
    // Set the global relay pointer for callbacks
    g_relay = relay;
    
    return relay;
}

int relay_run(relay_t *relay) {
    // Run the libwebsockets event loop
    while (g_relay) {
        lws_service(relay->lws_context, 100);
    }
    
    return 0;
}

void relay_cleanup(relay_t *relay) {
    if (relay) {
        // Destroy the libwebsockets context
        if (relay->lws_context) {
            lws_context_destroy(relay->lws_context);
        }
        
        // Close the database
        database_close(relay->db);
        
        // Reset the global relay pointer
        g_relay = NULL;
        
        // Free the relay structure
        free(relay);
    }
}

int relay_process_event(relay_t *relay, struct lws *wsi, json_t *event_json) {
    // Parse the event
    event_t *event = event_from_json(event_json);
    if (!event) {
        relay_send_ok(relay, wsi, "unknown", false, "invalid: event parsing failed");
        return -1;
    }
    
    // Verify the event
    if (!event_is_valid(event)) {
        char event_id[65] = "unknown";
        if (strlen(event->id) == 64) {
            strcpy(event_id, event->id);
        }
        relay_send_ok(relay, wsi, event_id, false, "invalid: event verification failed");
        event_free(event);
        return -1;
    }
    
    // Check if the event already exists
    if (database_event_exists(relay->db, event->id)) {
        relay_send_ok(relay, wsi, event->id, true, "duplicate: already have this event");
        event_free(event);
        return 0;
    }
    
    // Store the event in the database
    if (database_store_event(relay->db, event) != 0) {
        relay_send_ok(relay, wsi, event->id, false, "error: failed to store event");
        event_free(event);
        return -1;
    }
    
    // Send OK response
    relay_send_ok(relay, wsi, event->id, true, "");
    
    // Convert the event back to JSON to ensure consistent format
    json_t *event_json_normalized = event_to_json(event);
    
    // Find all active subscriptions that match this event
    // (This would require iterating through all active sessions and their subscriptions)
    // For simplicity, we'll skip that for now and only return to the sender
    
    // Clean up
    event_free(event);
    json_decref(event_json_normalized);
    
    return 0;
}

int relay_process_req(relay_t *relay, struct lws *wsi, const char *subscription_id, json_t *filters) {
    // Get the session data for this connection
    client_session_t *session = (client_session_t *)lws_wsi_user(wsi);
    
    // Check if this is a new subscription or a replacement for an existing one
    subscription_t *subscription = subscription_find_by_id(session->subscriptions, 
                                                        session->subscriptions_count, 
                                                        subscription_id, wsi);
    
    if (subscription) {
        // Replace the filters in the existing subscription
        subscription_add_filters(subscription, filters);
    } else {
        // Create a new subscription
        subscription = subscription_create(subscription_id, wsi, filters);
        if (!subscription) {
            relay_send_closed(relay, wsi, subscription_id, "error: could not create subscription");
            return -1;
        }
        
        // Add the subscription to the session
        if (subscription_add(&session->subscriptions, &session->subscriptions_count, subscription) != 0) {
            subscription_free(subscription);
            relay_send_closed(relay, wsi, subscription_id, "error: could not add subscription");
            return -1;
        }
    }
    
    // Query the database for events that match the filters
    json_t *events = json_array();
    
    // For each filter, query the database
    for (size_t i = 0; i < subscription->filters_count; i++) {
        json_t *filter = subscription->filters[i].filter;
        json_t *matching_events = NULL;
        
        // Get the limit parameter from the filter, or use the default
        json_t *limit_json = json_object_get(filter, "limit");
        int limit = relay->config->default_filter_limit;
        
        if (json_is_integer(limit_json)) {
            limit = json_integer_value(limit_json);
            if (limit > relay->config->max_filter_limit) {
                limit = relay->config->max_filter_limit;
            }
        }
        
        // Query the database with this filter
        database_query_events(relay->db, filter, &matching_events, limit);
        
        // Add the events to the result array
        if (matching_events) {
            size_t event_count = json_array_size(matching_events);
            for (size_t j = 0; j < event_count; j++) {
                json_array_append(events, json_array_get(matching_events, j));
            }
            json_decref(matching_events);
        }
    }
    
    // Send the events to the client
    size_t event_count = json_array_size(events);
    for (size_t i = 0; i < event_count; i++) {
        json_t *event = json_array_get(events, i);
        relay_send_event(relay, wsi, subscription_id, event);
    }
    
    // Send EOSE message
    relay_send_eose(relay, wsi, subscription_id);
    subscription->eose_sent = true;
    
    // Clean up
    json_decref(events);
    
    return 0;
}

int relay_process_close(relay_t *relay, struct lws *wsi, const char *subscription_id) {
    // Get the session data for this connection
    client_session_t *session = (client_session_t *)lws_wsi_user(wsi);
    
    // Remove the subscription
    if (subscription_remove(&session->subscriptions, &session->subscriptions_count, subscription_id, wsi) != 0) {
        // Subscription not found, but we don't need to send an error
        return -1;
    }
    
    return 0;
}

int relay_send_event(relay_t *relay, struct lws *wsi, const char *subscription_id, json_t *event_json) {
    json_t *response = json_create_event_response(subscription_id, event_json);
    if (!response) {
        return -1;
    }
    
    char *response_str = json_dumps(response, JSON_COMPACT);
    json_decref(response);
    
    if (!response_str) {
        return -1;
    }
    
    // Send the message
    size_t response_len = strlen(response_str);
    unsigned char *buf = malloc(LWS_PRE + response_len + 1);
    if (!buf) {
        free(response_str);
        return -1;
    }
    
    memcpy(buf + LWS_PRE, response_str, response_len);
    buf[LWS_PRE + response_len] = '\0';
    
    int n = lws_write(wsi, buf + LWS_PRE, response_len, LWS_WRITE_TEXT);
    
    free(buf);
    free(response_str);
    
    return (n < 0) ? -1 : 0;
}

int relay_send_ok(relay_t *relay, struct lws *wsi, const char *event_id, bool success, const char *message) {
    json_t *response = json_create_ok_response(event_id, success, message);
    if (!response) {
        return -1;
    }
    
    char *response_str = json_dumps(response, JSON_COMPACT);
    json_decref(response);
    
    if (!response_str) {
        return -1;
    }
    
    // Send the message
    size_t response_len = strlen(response_str);
    unsigned char *buf = malloc(LWS_PRE + response_len + 1);
    if (!buf) {
        free(response_str);
        return -1;
    }
    
    memcpy(buf + LWS_PRE, response_str, response_len);
    buf[LWS_PRE + response_len] = '\0';
    
    int n = lws_write(wsi, buf + LWS_PRE, response_len, LWS_WRITE_TEXT);
    
    free(buf);
    free(response_str);
    
    return (n < 0) ? -1 : 0;
}

int relay_send_eose(relay_t *relay, struct lws *wsi, const char *subscription_id) {
    json_t *response = json_create_eose_response(subscription_id);
    if (!response) {
        return -1;
    }
    
    char *response_str = json_dumps(response, JSON_COMPACT);
    json_decref(response);
    
    if (!response_str) {
        return -1;
    }
    
    // Send the message
    size_t response_len = strlen(response_str);
    unsigned char *buf = malloc(LWS_PRE + response_len + 1);
    if (!buf) {
        free(response_str);
        return -1;
    }
    
    memcpy(buf + LWS_PRE, response_str, response_len);
    buf[LWS_PRE + response_len] = '\0';
    
    int n = lws_write(wsi, buf + LWS_PRE, response_len, LWS_WRITE_TEXT);
    
    free(buf);
    free(response_str);
    
    return (n < 0) ? -1 : 0;
}

int relay_send_closed(relay_t *relay, struct lws *wsi, const char *subscription_id, const char *message) {
    json_t *response = json_create_closed_response(subscription_id, message);
    if (!response) {
        return -1;
    }
    
    char *response_str = json_dumps(response, JSON_COMPACT);
    json_decref(response);
    
    if (!response_str) {
        return -1;
    }
    
    // Send the message
    size_t response_len = strlen(response_str);
    unsigned char *buf = malloc(LWS_PRE + response_len + 1);
    if (!buf) {
        free(response_str);
        return -1;
    }
    
    memcpy(buf + LWS_PRE, response_str, response_len);
    buf[LWS_PRE + response_len] = '\0';
    
    int n = lws_write(wsi, buf + LWS_PRE, response_len, LWS_WRITE_TEXT);
    
    free(buf);
    free(response_str);
    
    return (n < 0) ? -1 : 0;
}

int relay_send_notice(relay_t *relay, struct lws *wsi, const char *message) {
    json_t *response = json_create_notice_response(message);
    if (!response) {
        return -1;
    }
    
    char *response_str = json_dumps(response, JSON_COMPACT);
    json_decref(response);
    
    if (!response_str) {
        return -1;
    }
    
    // Send the message
    size_t response_len = strlen(response_str);
    unsigned char *buf = malloc(LWS_PRE + response_len + 1);
    if (!buf) {
        free(response_str);
        return -1;
    }
    
    memcpy(buf + LWS_PRE, response_str, response_len);
    buf[LWS_PRE + response_len] = '\0';
    
    int n = lws_write(wsi, buf + LWS_PRE, response_len, LWS_WRITE_TEXT);
    
    free(buf);
    free(response_str);
    
    return (n < 0) ? -1 : 0;
} 