#ifndef CRELAY_RELAY_H
#define CRELAY_RELAY_H

#include <libwebsockets.h>
#include <jansson.h>
#include <sqlite3.h>
#include <stdbool.h>

// Forward declarations
struct subscription;
struct event;

// Relay configuration
typedef struct {
    const char *bind_address;
    int port;
    const char *db_path;
    int max_subscriptions_per_client;
    int max_filter_limit;
    int default_filter_limit;
} relay_config_t;

// Relay state
typedef struct {
    struct lws_context *lws_context;
    sqlite3 *db;
    const relay_config_t *config;
} relay_t;

// Initialize the relay with the given configuration
relay_t *relay_init(const relay_config_t *config);

// Start the relay main loop
int relay_run(relay_t *relay);

// Clean up and free resources
void relay_cleanup(relay_t *relay);

// Handle a new WebSocket connection
int relay_handle_connection(struct lws *wsi, relay_t *relay);

// Process an EVENT message from a client
int relay_process_event(relay_t *relay, struct lws *wsi, json_t *event_json);

// Process a REQ message from a client
int relay_process_req(relay_t *relay, struct lws *wsi, const char *subscription_id, json_t *filters);

// Process a CLOSE message from a client
int relay_process_close(relay_t *relay, struct lws *wsi, const char *subscription_id);

// Send an EVENT message to a client
int relay_send_event(relay_t *relay, struct lws *wsi, const char *subscription_id, json_t *event_json);

// Send an OK message to a client
int relay_send_ok(relay_t *relay, struct lws *wsi, const char *event_id, bool success, const char *message);

// Send an EOSE message to a client
int relay_send_eose(relay_t *relay, struct lws *wsi, const char *subscription_id);

// Send a CLOSED message to a client
int relay_send_closed(relay_t *relay, struct lws *wsi, const char *subscription_id, const char *message);

// Send a NOTICE message to a client
int relay_send_notice(relay_t *relay, struct lws *wsi, const char *message);

#endif // CRELAY_RELAY_H 