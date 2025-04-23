# Crelay - Nostr Relay in C

Crelay is a lightweight and efficient implementation of a [Nostr](https://github.com/nostr-protocol/nostr) relay written in C. This implementation follows the [NIP-01](https://github.com/nostr-protocol/nips/blob/master/01.md) specification for the basic protocol flow.

## Features

- Full compliance with NIP-01 specification
- WebSocket server for client connections
- Event validation and verification (Schnorr signatures)
- SQLite database for event storage
- Efficient filtering and subscription management
- Support for event tags and queries

## Dependencies

To build Crelay, you need the following dependencies:

- CMake (>= 3.10)
- libwebsockets
- jansson (JSON parsing)
- sqlite3
- libsecp256k1 (with Schnorr signature support)
- OpenSSL

### Installing Dependencies on Ubuntu/Debian

```bash
sudo apt update
sudo apt install cmake libwebsockets-dev libjansson-dev libsqlite3-dev libssl-dev
```

For libsecp256k1, you may need to build from source with Schnorr support:

```bash
git clone https://github.com/bitcoin-core/secp256k1.git
cd secp256k1
./autogen.sh
./configure --enable-module-schnorrsig --enable-experimental
make
sudo make install
```

### Installing Dependencies on macOS

```bash
brew install cmake libwebsockets jansson sqlite openssl
brew install bitcoin-core/bitcoin/secp256k1  # This should include Schnorr signatures
```

## Building

1. Clone the repository:

```bash
git clone https://github.com/melvincarvalho/crelay.git
cd crelay
```

2. Create a build directory and run CMake:

```bash
mkdir build
cd build
cmake ..
```

3. Build the project:

```bash
make
```

## Usage

Run the relay with default settings:

```bash
./crelay
```

### Command-line Options

- `--port PORT`: Specify the WebSocket port to listen on (default: 8080)
- `--db PATH`: Specify the SQLite database path (default: crelay.db)
- `--bind ADDRESS`: Specify the address to bind to (default: 0.0.0.0)
- `--help`: Show help message

## Using with Nostr Clients

Crelay follows the NIP-01 specification, so any Nostr client should be able to connect to it. Use the WebSocket URL format:

```
ws://your-server-address:8080
```

## Testing

You can test the relay using a Nostr client or with tools like WebSocket benchmarking tools.

### Example with websocat

```bash
# Subscribe to all events
echo '["REQ", "my-sub", {}]' | websocat ws://localhost:8080

# Publish an event (needs a signed event according to the Nostr protocol)
echo '["EVENT", {...event object...}]' | websocat ws://localhost:8080

# Close a subscription
echo '["CLOSE", "my-sub"]' | websocat ws://localhost:8080
```

## Architecture

Crelay is structured into several components:

- **relay**: Main server logic and WebSocket handling
- **event**: Event validation, serialization, and verification
- **database**: SQLite storage for events and efficient querying
- **subscription**: Subscription management and filtering
- **crypto**: Cryptographic operations (SHA-256, Schnorr signatures)
- **json_utils**: JSON handling utilities

## Contributing

Contributions are welcome! Feel free to submit issues or pull requests.

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Acknowledgments

- The [Nostr Protocol](https://github.com/nostr-protocol/nostr) for the innovative decentralized social network concept
- All the developers working on the Nostr protocol specifications
