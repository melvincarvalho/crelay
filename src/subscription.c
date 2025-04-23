#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <jansson.h>
#include "../include/subscription.h"
#include "../include/event.h"

// Create a new subscription
subscription_t *subscription_create(const char *id, struct lws *wsi, json_t *filters_json) {
    if (!id || !wsi || !filters_json || !json_is_array(filters_json)) {
        return NULL;
    }
    
    // Validate subscription ID length
    if (strlen(id) > MAX_SUBSCRIPTION_ID_LENGTH) {
        return NULL;
    }
    
    // Allocate the subscription
    subscription_t *subscription = calloc(1, sizeof(subscription_t));
    if (!subscription) {
        return NULL;
    }
    
    // Copy the ID
    strcpy(subscription->id, id);
    
    // Set the WebSocket connection
    subscription->wsi = wsi;
    
    // Add filters
    if (subscription_add_filters(subscription, filters_json) != 0) {
        subscription_free(subscription);
        return NULL;
    }
    
    return subscription;
}

// Free a subscription
void subscription_free(subscription_t *subscription) {
    if (subscription) {
        // Free filters
        for (size_t i = 0; i < subscription->filters_count; i++) {
            json_decref(subscription->filters[i].filter);
        }
        free(subscription->filters);
        
        // Free the subscription itself
        free(subscription);
    }
}

// Check if an event matches any of the subscription's filters
bool subscription_matches_event(const subscription_t *subscription, const event_t *event) {
    if (!subscription || !event) {
        return false;
    }
    
    // Check each filter
    for (size_t i = 0; i < subscription->filters_count; i++) {
        if (event_matches_filter(event, subscription->filters[i].filter)) {
            return true;
        }
    }
    
    return false;
}

// Add a set of filters to an existing subscription (replacing previous filters)
int subscription_add_filters(subscription_t *subscription, json_t *filters_json) {
    if (!subscription || !filters_json || !json_is_array(filters_json)) {
        return -1;
    }
    
    // Free existing filters if any
    for (size_t i = 0; i < subscription->filters_count; i++) {
        json_decref(subscription->filters[i].filter);
    }
    free(subscription->filters);
    
    // Count valid filters
    size_t valid_filters = 0;
    size_t size = json_array_size(filters_json);
    
    for (size_t i = 0; i < size; i++) {
        json_t *filter = json_array_get(filters_json, i);
        if (json_is_object(filter)) {
            valid_filters++;
        }
    }
    
    // Allocate memory for filters
    subscription->filters = calloc(valid_filters, sizeof(subscription_filter_t));
    if (!subscription->filters && valid_filters > 0) {
        subscription->filters_count = 0;
        return -1;
    }
    
    // Copy filters
    size_t filter_index = 0;
    for (size_t i = 0; i < size; i++) {
        json_t *filter = json_array_get(filters_json, i);
        if (json_is_object(filter)) {
            subscription->filters[filter_index].filter = json_incref(filter);
            filter_index++;
        }
    }
    
    subscription->filters_count = valid_filters;
    
    return 0;
}

// Find a subscription by ID for a specific WebSocket connection
subscription_t *subscription_find_by_id(subscription_t **subscriptions, size_t count, 
                                      const char *id, struct lws *wsi) {
    if (!subscriptions || !id || !wsi) {
        return NULL;
    }
    
    for (size_t i = 0; i < count; i++) {
        if (subscriptions[i] && 
            subscriptions[i]->wsi == wsi && 
            strcmp(subscriptions[i]->id, id) == 0) {
            return subscriptions[i];
        }
    }
    
    return NULL;
}

// Add a subscription to a list of subscriptions
int subscription_add(subscription_t ***subscriptions, size_t *count, subscription_t *new_subscription) {
    if (!subscriptions || !count || !new_subscription) {
        return -1;
    }
    
    // Allocate or reallocate the array
    subscription_t **new_array = realloc(*subscriptions, (*count + 1) * sizeof(subscription_t *));
    if (!new_array) {
        return -1;
    }
    
    // Add the new subscription
    new_array[*count] = new_subscription;
    *subscriptions = new_array;
    (*count)++;
    
    return 0;
}

// Remove a subscription from a list of subscriptions
int subscription_remove(subscription_t ***subscriptions, size_t *count, 
                      const char *id, struct lws *wsi) {
    if (!subscriptions || !count || !id || !wsi || *count == 0) {
        return -1;
    }
    
    // Find the subscription
    int index = -1;
    for (size_t i = 0; i < *count; i++) {
        if ((*subscriptions)[i] && 
            (*subscriptions)[i]->wsi == wsi && 
            strcmp((*subscriptions)[i]->id, id) == 0) {
            index = i;
            break;
        }
    }
    
    if (index == -1) {
        return -1;  // Subscription not found
    }
    
    // Free the subscription
    subscription_free((*subscriptions)[index]);
    
    // Move the last subscription to this position (if not the last)
    if (index < (int)(*count) - 1) {
        (*subscriptions)[index] = (*subscriptions)[*count - 1];
    }
    
    // Reduce the count
    (*count)--;
    
    // Reallocate the array if count is zero (free it)
    if (*count == 0) {
        free(*subscriptions);
        *subscriptions = NULL;
    } else if (*count > 0) {
        // Optional: Shrink the array
        subscription_t **new_array = realloc(*subscriptions, *count * sizeof(subscription_t *));
        if (new_array) {
            *subscriptions = new_array;
        }
    }
    
    return 0;
}

// Remove all subscriptions for a specific WebSocket connection
int subscription_remove_by_wsi(subscription_t ***subscriptions, size_t *count, struct lws *wsi) {
    if (!subscriptions || !count || !wsi || *count == 0) {
        return -1;
    }
    
    // Count subscriptions to remove
    size_t remove_count = 0;
    for (size_t i = 0; i < *count; i++) {
        if ((*subscriptions)[i] && (*subscriptions)[i]->wsi == wsi) {
            remove_count++;
        }
    }
    
    if (remove_count == 0) {
        return 0;  // No subscriptions to remove
    }
    
    // Create a new array for the remaining subscriptions
    size_t new_count = *count - remove_count;
    subscription_t **new_array = NULL;
    
    if (new_count > 0) {
        new_array = malloc(new_count * sizeof(subscription_t *));
        if (!new_array) {
            return -1;
        }
    }
    
    // Copy the subscriptions to keep
    size_t new_index = 0;
    for (size_t i = 0; i < *count; i++) {
        if ((*subscriptions)[i] && (*subscriptions)[i]->wsi != wsi) {
            new_array[new_index++] = (*subscriptions)[i];
        } else {
            subscription_free((*subscriptions)[i]);
        }
    }
    
    // Replace the old array with the new one
    free(*subscriptions);
    *subscriptions = new_array;
    *count = new_count;
    
    return 0;
} 