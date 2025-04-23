#ifndef CRELAY_DATABASE_H
#define CRELAY_DATABASE_H

#include <sqlite3.h>
#include <jansson.h>
#include <stdbool.h>
#include "event.h"

// Initialize the database, creating tables if they don't exist
int database_init(sqlite3 **db, const char *db_path);

// Store an event in the database
int database_store_event(sqlite3 *db, const event_t *event);

// Query events based on a filter
int database_query_events(sqlite3 *db, json_t *filter, json_t **events, int limit);

// Check if an event exists in the database
bool database_event_exists(sqlite3 *db, const char *event_id);

// Close the database
void database_close(sqlite3 *db);

// Get the latest event of a specific kind for a pubkey (for replaceable events)
json_t *database_get_latest_replaceable_event(sqlite3 *db, int kind, const char *pubkey);

// Get the latest event with a specific d tag for a pubkey and kind (for parameterized replaceable events)
json_t *database_get_latest_parameterized_replaceable_event(sqlite3 *db, int kind, 
                                                         const char *pubkey, 
                                                         const char *d_tag_value);

// Delete old events (for cleanup/maintenance)
int database_delete_old_events(sqlite3 *db, uint64_t before_timestamp);

#endif // CRELAY_DATABASE_H 