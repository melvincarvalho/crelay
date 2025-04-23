#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sqlite3.h>
#include <jansson.h>
#include "../include/database.h"
#include "../include/event.h"

// Initialize the database, creating tables if they don't exist
int database_init(sqlite3 **db, const char *db_path) {
    // Open the database
    int rc = sqlite3_open(db_path, db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to open database: %s\n", sqlite3_errmsg(*db));
        sqlite3_close(*db);
        return -1;
    }
    
    // Create the events table
    const char *create_events_table = 
        "CREATE TABLE IF NOT EXISTS events ("
        "id TEXT PRIMARY KEY,"
        "pubkey TEXT NOT NULL,"
        "created_at INTEGER NOT NULL,"
        "kind INTEGER NOT NULL,"
        "content TEXT NOT NULL,"
        "sig TEXT NOT NULL,"
        "tags_json TEXT NOT NULL"
        ");";
    
    char *err_msg = NULL;
    rc = sqlite3_exec(*db, create_events_table, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create events table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(*db);
        return -1;
    }
    
    // Create indexes for faster queries
    const char *create_indexes[] = {
        "CREATE INDEX IF NOT EXISTS idx_events_pubkey ON events (pubkey);",
        "CREATE INDEX IF NOT EXISTS idx_events_kind ON events (kind);",
        "CREATE INDEX IF NOT EXISTS idx_events_created_at ON events (created_at);",
        "CREATE INDEX IF NOT EXISTS idx_events_pubkey_kind ON events (pubkey, kind);",
        "CREATE INDEX IF NOT EXISTS idx_events_kind_created_at ON events (kind, created_at);",
        NULL
    };
    
    for (int i = 0; create_indexes[i] != NULL; i++) {
        rc = sqlite3_exec(*db, create_indexes[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to create index: %s\n", err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(*db);
            return -1;
        }
    }
    
    // Create a virtual table for full-text search on content
    const char *create_fts_table = 
        "CREATE VIRTUAL TABLE IF NOT EXISTS events_fts USING fts5("
        "content,"
        "content=events,"
        "content_rowid=rowid"
        ");";
    
    rc = sqlite3_exec(*db, create_fts_table, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create FTS table: %s\n", err_msg);
        sqlite3_free(err_msg);
        // Continue anyway, as FTS is not critical
    }
    
    // Create a table for tag indexing
    const char *create_tags_table = 
        "CREATE TABLE IF NOT EXISTS event_tags ("
        "event_id TEXT NOT NULL,"
        "tag_name TEXT NOT NULL,"
        "tag_value TEXT NOT NULL,"
        "PRIMARY KEY (event_id, tag_name, tag_value),"
        "FOREIGN KEY (event_id) REFERENCES events (id) ON DELETE CASCADE"
        ");";
    
    rc = sqlite3_exec(*db, create_tags_table, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to create tags table: %s\n", err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(*db);
        return -1;
    }
    
    // Create indexes for tag queries
    const char *create_tag_indexes[] = {
        "CREATE INDEX IF NOT EXISTS idx_event_tags_tag_name ON event_tags (tag_name);",
        "CREATE INDEX IF NOT EXISTS idx_event_tags_tag_value ON event_tags (tag_value);",
        "CREATE INDEX IF NOT EXISTS idx_event_tags_tag_name_value ON event_tags (tag_name, tag_value);",
        NULL
    };
    
    for (int i = 0; create_tag_indexes[i] != NULL; i++) {
        rc = sqlite3_exec(*db, create_tag_indexes[i], NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            fprintf(stderr, "Failed to create tag index: %s\n", err_msg);
            sqlite3_free(err_msg);
            sqlite3_close(*db);
            return -1;
        }
    }
    
    return 0;
}

// Store an event in the database
int database_store_event(sqlite3 *db, const event_t *event) {
    if (!db || !event) {
        return -1;
    }
    
    // Begin a transaction
    sqlite3_exec(db, "BEGIN TRANSACTION", NULL, NULL, NULL);
    
    // Insert into the events table
    sqlite3_stmt *stmt;
    const char *sql = "INSERT INTO events (id, pubkey, created_at, kind, content, sig, tags_json) "
                     "VALUES (?, ?, ?, ?, ?, ?, ?)";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    
    // Serialize tags to JSON
    json_t *tags_array = json_array();
    for (size_t i = 0; i < event->tags_count; i++) {
        json_t *tag_array = json_array();
        for (size_t j = 0; j < event->tags[i].count; j++) {
            json_array_append_new(tag_array, json_string(event->tags[i].values[j]));
        }
        json_array_append_new(tags_array, tag_array);
    }
    char *tags_json = json_dumps(tags_array, JSON_COMPACT);
    json_decref(tags_array);
    
    // Bind parameters
    sqlite3_bind_text(stmt, 1, event->id, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, event->pubkey, -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 3, event->created_at);
    sqlite3_bind_int(stmt, 4, event->kind);
    sqlite3_bind_text(stmt, 5, event->content, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 6, event->sig, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 7, tags_json, -1, SQLITE_TRANSIENT);
    
    // Execute the statement
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    free(tags_json);
    
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert event: %s\n", sqlite3_errmsg(db));
        sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
        return -1;
    }
    
    // Insert event tags for indexing
    for (size_t i = 0; i < event->tags_count; i++) {
        if (event->tags[i].count >= 2) {
            const char *tag_name = event->tags[i].values[0];
            
            // Only index single-letter tag names
            if (strlen(tag_name) == 1 && 
                ((tag_name[0] >= 'a' && tag_name[0] <= 'z') || 
                 (tag_name[0] >= 'A' && tag_name[0] <= 'Z'))) {
                
                const char *tag_value = event->tags[i].values[1];
                
                const char *sql_tag = "INSERT OR IGNORE INTO event_tags (event_id, tag_name, tag_value) "
                                     "VALUES (?, ?, ?)";
                
                rc = sqlite3_prepare_v2(db, sql_tag, -1, &stmt, NULL);
                if (rc != SQLITE_OK) {
                    fprintf(stderr, "Failed to prepare tag statement: %s\n", sqlite3_errmsg(db));
                    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
                    return -1;
                }
                
                sqlite3_bind_text(stmt, 1, event->id, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, tag_name, -1, SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, tag_value, -1, SQLITE_STATIC);
                
                rc = sqlite3_step(stmt);
                sqlite3_finalize(stmt);
                
                if (rc != SQLITE_DONE) {
                    fprintf(stderr, "Failed to insert tag: %s\n", sqlite3_errmsg(db));
                    sqlite3_exec(db, "ROLLBACK", NULL, NULL, NULL);
                    return -1;
                }
            }
        }
    }
    
    // Update the FTS table
    const char *sql_fts = "INSERT INTO events_fts(rowid, content) "
                          "SELECT rowid, content FROM events WHERE id = ?";
    
    rc = sqlite3_prepare_v2(db, sql_fts, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, event->id, -1, SQLITE_STATIC);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
    
    // Commit the transaction
    sqlite3_exec(db, "COMMIT", NULL, NULL, NULL);
    
    return 0;
}

// Build a SQL query from a filter
static char *build_filter_query(json_t *filter, sqlite3 *db, sqlite3_stmt **stmt, int limit) {
    char *query = NULL;
    size_t query_size = 0;
    
    // Base query
    query = sqlite3_mprintf("SELECT e.id, e.pubkey, e.created_at, e.kind, e.content, e.sig, e.tags_json "
                           "FROM events e WHERE 1=1");
    query_size = strlen(query);
    
    if (!query) {
        return NULL;
    }
    
    // Add filters
    if (json_object_get(filter, "ids") && json_is_array(json_object_get(filter, "ids"))) {
        json_t *ids = json_object_get(filter, "ids");
        size_t size = json_array_size(ids);
        
        if (size > 0) {
            char *temp = sqlite3_mprintf("%s AND e.id IN (", query);
            sqlite3_free(query);
            query = temp;
            
            for (size_t i = 0; i < size; i++) {
                json_t *id = json_array_get(ids, i);
                if (json_is_string(id)) {
                    if (i > 0) {
                        temp = sqlite3_mprintf("%s, '%q'", query, json_string_value(id));
                    } else {
                        temp = sqlite3_mprintf("%s'%q'", query, json_string_value(id));
                    }
                    sqlite3_free(query);
                    query = temp;
                }
            }
            
            temp = sqlite3_mprintf("%s)", query);
            sqlite3_free(query);
            query = temp;
        }
    }
    
    if (json_object_get(filter, "authors") && json_is_array(json_object_get(filter, "authors"))) {
        json_t *authors = json_object_get(filter, "authors");
        size_t size = json_array_size(authors);
        
        if (size > 0) {
            char *temp = sqlite3_mprintf("%s AND e.pubkey IN (", query);
            sqlite3_free(query);
            query = temp;
            
            for (size_t i = 0; i < size; i++) {
                json_t *author = json_array_get(authors, i);
                if (json_is_string(author)) {
                    if (i > 0) {
                        temp = sqlite3_mprintf("%s, '%q'", query, json_string_value(author));
                    } else {
                        temp = sqlite3_mprintf("%s'%q'", query, json_string_value(author));
                    }
                    sqlite3_free(query);
                    query = temp;
                }
            }
            
            temp = sqlite3_mprintf("%s)", query);
            sqlite3_free(query);
            query = temp;
        }
    }
    
    if (json_object_get(filter, "kinds") && json_is_array(json_object_get(filter, "kinds"))) {
        json_t *kinds = json_object_get(filter, "kinds");
        size_t size = json_array_size(kinds);
        
        if (size > 0) {
            char *temp = sqlite3_mprintf("%s AND e.kind IN (", query);
            sqlite3_free(query);
            query = temp;
            
            for (size_t i = 0; i < size; i++) {
                json_t *kind = json_array_get(kinds, i);
                if (json_is_integer(kind)) {
                    if (i > 0) {
                        temp = sqlite3_mprintf("%s, %lld", query, json_integer_value(kind));
                    } else {
                        temp = sqlite3_mprintf("%s%lld", query, json_integer_value(kind));
                    }
                    sqlite3_free(query);
                    query = temp;
                }
            }
            
            temp = sqlite3_mprintf("%s)", query);
            sqlite3_free(query);
            query = temp;
        }
    }
    
    if (json_object_get(filter, "since") && json_is_integer(json_object_get(filter, "since"))) {
        char *temp = sqlite3_mprintf("%s AND e.created_at >= %lld", query, 
                                   json_integer_value(json_object_get(filter, "since")));
        sqlite3_free(query);
        query = temp;
    }
    
    if (json_object_get(filter, "until") && json_is_integer(json_object_get(filter, "until"))) {
        char *temp = sqlite3_mprintf("%s AND e.created_at <= %lld", query, 
                                   json_integer_value(json_object_get(filter, "until")));
        sqlite3_free(query);
        query = temp;
    }
    
    // Process tag filters
    const char *key;
    json_t *value;
    int tag_filters = 0;
    
    json_object_foreach(filter, key, value) {
        if (key[0] == '#' && strlen(key) >= 2 && json_is_array(value) && json_array_size(value) > 0) {
            char tag_name = key[1];
            
            if (tag_filters == 0) {
                // First tag filter, add join
                char *temp = sqlite3_mprintf("%s AND e.id IN (SELECT event_id FROM event_tags WHERE ", query);
                sqlite3_free(query);
                query = temp;
            } else {
                // Additional tag filter, add AND
                char *temp = sqlite3_mprintf("%s AND event_id IN (SELECT event_id FROM event_tags WHERE ", query);
                sqlite3_free(query);
                query = temp;
            }
            
            char *temp = sqlite3_mprintf("%s tag_name = '%c' AND tag_value IN (", query, tag_name);
            sqlite3_free(query);
            query = temp;
            
            size_t size = json_array_size(value);
            for (size_t i = 0; i < size; i++) {
                json_t *tag_value = json_array_get(value, i);
                if (json_is_string(tag_value)) {
                    if (i > 0) {
                        temp = sqlite3_mprintf("%s, '%q'", query, json_string_value(tag_value));
                    } else {
                        temp = sqlite3_mprintf("%s'%q'", query, json_string_value(tag_value));
                    }
                    sqlite3_free(query);
                    query = temp;
                }
            }
            
            temp = sqlite3_mprintf("%s))", query);
            sqlite3_free(query);
            query = temp;
            
            tag_filters++;
        }
    }
    
    // Add ordering and limit
    char *temp = sqlite3_mprintf("%s ORDER BY e.created_at DESC LIMIT %d", query, limit);
    sqlite3_free(query);
    query = temp;
    
    // Prepare the statement
    int rc = sqlite3_prepare_v2(db, query, -1, stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare query: %s\n", sqlite3_errmsg(db));
        fprintf(stderr, "Query: %s\n", query);
        sqlite3_free(query);
        return NULL;
    }
    
    return query;
}

// Query events based on a filter
int database_query_events(sqlite3 *db, json_t *filter, json_t **events, int limit) {
    if (!db || !filter || !events) {
        return -1;
    }
    
    sqlite3_stmt *stmt = NULL;
    char *query = build_filter_query(filter, db, &stmt, limit);
    
    if (!query || !stmt) {
        if (query) {
            sqlite3_free(query);
        }
        return -1;
    }
    
    // Create a JSON array for the results
    *events = json_array();
    
    // Execute the query and collect results
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        // Extract values from the result row
        const char *id = (const char *)sqlite3_column_text(stmt, 0);
        const char *pubkey = (const char *)sqlite3_column_text(stmt, 1);
        sqlite3_int64 created_at = sqlite3_column_int64(stmt, 2);
        int kind = sqlite3_column_int(stmt, 3);
        const char *content = (const char *)sqlite3_column_text(stmt, 4);
        const char *sig = (const char *)sqlite3_column_text(stmt, 5);
        const char *tags_json_str = (const char *)sqlite3_column_text(stmt, 6);
        
        // Parse the tags JSON
        json_error_t error;
        json_t *tags_json = json_loads(tags_json_str, 0, &error);
        
        if (!tags_json) {
            fprintf(stderr, "Error parsing tags JSON: %s\n", error.text);
            continue;
        }
        
        // Create a JSON object for this event
        json_t *event = json_object();
        json_object_set_new(event, "id", json_string(id));
        json_object_set_new(event, "pubkey", json_string(pubkey));
        json_object_set_new(event, "created_at", json_integer(created_at));
        json_object_set_new(event, "kind", json_integer(kind));
        json_object_set_new(event, "content", json_string(content));
        json_object_set_new(event, "sig", json_string(sig));
        json_object_set(event, "tags", tags_json);
        
        // Add the event to the results array
        json_array_append_new(*events, event);
        
        // Clean up
        json_decref(tags_json);
    }
    
    // Clean up
    sqlite3_finalize(stmt);
    sqlite3_free(query);
    
    return 0;
}

// Check if an event exists in the database
bool database_event_exists(sqlite3 *db, const char *event_id) {
    if (!db || !event_id) {
        return false;
    }
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT 1 FROM events WHERE id = ?";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return false;
    }
    
    sqlite3_bind_text(stmt, 1, event_id, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    bool exists = (rc == SQLITE_ROW);
    
    sqlite3_finalize(stmt);
    
    return exists;
}

// Close the database
void database_close(sqlite3 *db) {
    if (db) {
        sqlite3_close(db);
    }
}

// Get the latest event of a specific kind for a pubkey (for replaceable events)
json_t *database_get_latest_replaceable_event(sqlite3 *db, int kind, const char *pubkey) {
    if (!db || !pubkey) {
        return NULL;
    }
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT id, pubkey, created_at, kind, content, sig, tags_json "
                     "FROM events WHERE kind = ? AND pubkey = ? "
                     "ORDER BY created_at DESC LIMIT 1";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, kind);
    sqlite3_bind_text(stmt, 2, pubkey, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Extract values from the result row
    const char *id = (const char *)sqlite3_column_text(stmt, 0);
    // pubkey is already known
    sqlite3_int64 created_at = sqlite3_column_int64(stmt, 2);
    // kind is already known
    const char *content = (const char *)sqlite3_column_text(stmt, 4);
    const char *sig = (const char *)sqlite3_column_text(stmt, 5);
    const char *tags_json_str = (const char *)sqlite3_column_text(stmt, 6);
    
    // Parse the tags JSON
    json_error_t error;
    json_t *tags_json = json_loads(tags_json_str, 0, &error);
    
    if (!tags_json) {
        fprintf(stderr, "Error parsing tags JSON: %s\n", error.text);
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Create a JSON object for this event
    json_t *event = json_object();
    json_object_set_new(event, "id", json_string(id));
    json_object_set_new(event, "pubkey", json_string(pubkey));
    json_object_set_new(event, "created_at", json_integer(created_at));
    json_object_set_new(event, "kind", json_integer(kind));
    json_object_set_new(event, "content", json_string(content));
    json_object_set_new(event, "sig", json_string(sig));
    json_object_set(event, "tags", tags_json);
    
    // Clean up
    sqlite3_finalize(stmt);
    json_decref(tags_json);
    
    return event;
}

// Get the latest event with a specific d tag for a pubkey and kind (for parameterized replaceable events)
json_t *database_get_latest_parameterized_replaceable_event(sqlite3 *db, int kind, 
                                                         const char *pubkey, 
                                                         const char *d_tag_value) {
    if (!db || !pubkey || !d_tag_value) {
        return NULL;
    }
    
    sqlite3_stmt *stmt;
    const char *sql = "SELECT e.id, e.pubkey, e.created_at, e.kind, e.content, e.sig, e.tags_json "
                     "FROM events e "
                     "JOIN event_tags t ON e.id = t.event_id "
                     "WHERE e.kind = ? AND e.pubkey = ? AND t.tag_name = 'd' AND t.tag_value = ? "
                     "ORDER BY e.created_at DESC LIMIT 1";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return NULL;
    }
    
    sqlite3_bind_int(stmt, 1, kind);
    sqlite3_bind_text(stmt, 2, pubkey, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, d_tag_value, -1, SQLITE_STATIC);
    
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_ROW) {
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Extract values from the result row
    const char *id = (const char *)sqlite3_column_text(stmt, 0);
    // pubkey is already known
    sqlite3_int64 created_at = sqlite3_column_int64(stmt, 2);
    // kind is already known
    const char *content = (const char *)sqlite3_column_text(stmt, 4);
    const char *sig = (const char *)sqlite3_column_text(stmt, 5);
    const char *tags_json_str = (const char *)sqlite3_column_text(stmt, 6);
    
    // Parse the tags JSON
    json_error_t error;
    json_t *tags_json = json_loads(tags_json_str, 0, &error);
    
    if (!tags_json) {
        fprintf(stderr, "Error parsing tags JSON: %s\n", error.text);
        sqlite3_finalize(stmt);
        return NULL;
    }
    
    // Create a JSON object for this event
    json_t *event = json_object();
    json_object_set_new(event, "id", json_string(id));
    json_object_set_new(event, "pubkey", json_string(pubkey));
    json_object_set_new(event, "created_at", json_integer(created_at));
    json_object_set_new(event, "kind", json_integer(kind));
    json_object_set_new(event, "content", json_string(content));
    json_object_set_new(event, "sig", json_string(sig));
    json_object_set(event, "tags", tags_json);
    
    // Clean up
    sqlite3_finalize(stmt);
    json_decref(tags_json);
    
    return event;
}

// Delete old events (for cleanup/maintenance)
int database_delete_old_events(sqlite3 *db, uint64_t before_timestamp) {
    if (!db || before_timestamp == 0) {
        return -1;
    }
    
    sqlite3_stmt *stmt;
    const char *sql = "DELETE FROM events WHERE created_at < ?";
    
    int rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement: %s\n", sqlite3_errmsg(db));
        return -1;
    }
    
    sqlite3_bind_int64(stmt, 1, before_timestamp);
    
    rc = sqlite3_step(stmt);
    
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to delete old events: %s\n", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        return -1;
    }
    
    sqlite3_finalize(stmt);
    
    return 0;
} 