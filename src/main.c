#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include "../include/relay.h"

static volatile int running = 1;

static void sigint_handler(int sig) {
    running = 0;
}

int main(int argc, char *argv[]) {
    // Default configuration
    relay_config_t config = {
        .bind_address = "0.0.0.0",
        .port = 8080,
        .db_path = "crelay.db",
        .max_subscriptions_per_client = 20,
        .max_filter_limit = 1000,
        .default_filter_limit = 100
    };
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            config.port = atoi(argv[i + 1]);
            i++;
        } else if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            config.db_path = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--bind") == 0 && i + 1 < argc) {
            config.bind_address = argv[i + 1];
            i++;
        } else if (strcmp(argv[i], "--help") == 0) {
            printf("Usage: %s [options]\n", argv[0]);
            printf("Options:\n");
            printf("  --port PORT       Port to listen on (default: 8080)\n");
            printf("  --db PATH         Path to SQLite database (default: crelay.db)\n");
            printf("  --bind ADDRESS    Address to bind to (default: 0.0.0.0)\n");
            printf("  --help            Show this help message\n");
            return 0;
        }
    }
    
    printf("Crelay - Nostr Relay (NIP-01)\n");
    printf("Listening on %s:%d\n", config.bind_address, config.port);
    printf("Database: %s\n", config.db_path);
    
    // Set up signal handler
    signal(SIGINT, sigint_handler);
    
    // Initialize the relay
    relay_t *relay = relay_init(&config);
    if (!relay) {
        fprintf(stderr, "Failed to initialize relay\n");
        return 1;
    }
    
    // Run the relay until interrupted
    int result = relay_run(relay);
    
    // Clean up
    relay_cleanup(relay);
    
    printf("Relay shutdown complete\n");
    return result;
} 