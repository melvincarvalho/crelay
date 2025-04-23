#ifndef CRELAY_SUBSCRIPTION_H
#define CRELAY_SUBSCRIPTION_H

#include <libwebsockets.h>
#include <jansson.h>
#include <stdbool.h>
#include "event.h"

// Structure for a subscription filter
typedef struct {
    json_t *filter;  // JSON object containing filter criteria
} subscription_filter_t;

// Structure for a subscription
typedef struct {
    char id[MAX_SUBSCRIPTION_ID_LENGTH + 1];  // Subscription ID
    struct lws *wsi;                         // WebSocket connection
    subscription_filter_t *filters;          // Array of filters
    size_t filters_count;                    // Number of filters
    bool eose_sent;                          // Has EOSE been sent for this subscription?
} subscription_t;

// Create a new subscription
subscription_t *subscription_create(const char *id, struct lws *wsi, json_t *filters_json);

// Free a subscription
void subscription_free(subscription_t *subscription);

// Check if an event matches any of the subscription's filters
bool subscription_matches_event(const subscription_t *subscription, const event_t *event);

// Add a set of filters to an existing subscription (replacing previous filters)
int subscription_add_filters(subscription_t *subscription, json_t *filters_json);

// Find a subscription by ID for a specific WebSocket connection
subscription_t *subscription_find_by_id(subscription_t **subscriptions, size_t count, 
                                      const char *id, struct lws *wsi);

// Add a subscription to a list of subscriptions
int subscription_add(subscription_t ***subscriptions, size_t *count, subscription_t *new_subscription);

// Remove a subscription from a list of subscriptions
int subscription_remove(subscription_t ***subscriptions, size_t *count, 
                      const char *id, struct lws *wsi);

// Remove all subscriptions for a specific WebSocket connection
int subscription_remove_by_wsi(subscription_t ***subscriptions, size_t *count, struct lws *wsi);

#endif // CRELAY_SUBSCRIPTION_H 