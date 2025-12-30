# FC Library - Claude Code Project Guide

## Project Overview

FC (Fast-Compiling C++ Library) is a high-performance utility library for asynchronous C++ programming. It provides:

- Cooperative multi-tasking with futures, mutexes, and signals
- Boost ASIO wrapper for async operations with synchronous-style coding
- C++ reflection system enabling automatic JSON/Binary serialization
- Automatic client/server stub generation with JSON-RPC support
- Cryptographic primitives (hashes, ECC, AES, base encodings)
- Comprehensive logging infrastructure
- Wrapped Boost APIs (filesystem, threading, exception handling)

## Tech Stack

- **Language**: C++ (C++11+)
- **Build System**: CMake 2.8.12+
- **Platforms**: Windows (MSVC, MinGW), macOS, Linux

### Core Dependencies
- Boost (ASIO, filesystem, thread, exception, multi_index_container)
- OpenSSL (cryptography)
- secp256k1-zkp (elliptic curve - vendored submodule)
- Zlib & BZip2 (compression)
- Readline/Curses (optional, CLI)

### Vendored Libraries (`vendor/`)
- `secp256k1-zkp/` - Bitcoin's ECC library with ZK proofs (submodule)
- `websocketpp/` - WebSocket protocol (submodule)
- `equihash/` - Proof-of-work hashing
- `simdjson/` - SIMD-accelerated JSON parser
- `cyoencode-1.0.2/` - Base encoding/decoding

## Directory Structure

```
fc/
├── include/fc/           # Public headers (148 .hpp files)
│   ├── api.hpp           # API framework & RPC system
│   ├── variant.hpp       # Dynamic type system (cornerstone)
│   ├── crypto/           # Cryptography (20 headers)
│   ├── thread/           # Threading & futures (12 headers)
│   ├── network/          # Networking (11 headers)
│   ├── io/               # I/O streams, JSON (6 headers)
│   ├── rpc/              # RPC framework (6 headers)
│   ├── log/              # Logging system
│   ├── reflect/          # Reflection system
│   └── compress/         # Compression utilities
├── src/                  # Implementation (100 .cpp files)
│   ├── crypto/           # Hash, ECC, encoding implementations
│   ├── thread/           # Threading primitives
│   ├── network/          # Socket implementations
│   ├── io/               # Serialization implementations
│   ├── rpc/              # RPC layer
│   └── log/              # Logging implementations
├── tests/                # Test suite (32 test files)
├── vendor/               # Third-party libraries
├── CMakeModules/         # Custom CMake modules
└── GitVersionGen/        # Git version tracking
```

## Development Commands

### Building

```bash
# Standard build
mkdir build && cd build
cmake ..
make -j$(nproc)

# With specific ECC implementation
cmake -DECC_IMPL=secp256k1 ..  # default
cmake -DECC_IMPL=openssl ..
cmake -DECC_IMPL=mixed ..

# Minimal build (no threading, RPC, network)
cmake -DHIVE_BUILD_ON_MINIMAL_FC=ON ..

# Unity build (faster compilation)
cmake -DUNITY_BUILD=ON ..

# With Valgrind annotations
cmake -DUSE_VALGRIND=ON ..
```

### CMake Options

| Option | Default | Description |
|--------|---------|-------------|
| `ECC_IMPL` | secp256k1 | ECC implementation: secp256k1, openssl, mixed |
| `HIVE_BUILD_ON_MINIMAL_FC` | OFF | Build minimal version |
| `UNITY_BUILD` | OFF | Enable unity builds |
| `LOG_LONG_API` | ON | Log slow API calls |
| `LOG_LONG_API_MAX_MS` | 1000 | Max API execution time (ms) |
| `LOG_LONG_API_WARN_MS` | 750 | Warning threshold (ms) |
| `USE_VALGRIND` | OFF | Build with Valgrind annotations |

### Running Tests

```bash
cd build

# Run comprehensive test suite
./tests/all_tests

# Individual tests
./tests/ecc_test           # ECC interoperability
./tests/hash_benchmark     # Hash algorithm benchmarks
./tests/thread_test        # Threading tests
./tests/task_cancel_test   # Task cancellation tests
./tests/ntp_test           # NTP tests
./tests/bloom_test         # Bloom filter tests
./tests/sha_test           # SHA hash tests
./tests/hmac_test          # HMAC tests
```

### ECC Interoperability Testing
```bash
./tests/ecc_test <password> <interop_file>
./tests/crypto/ecc-interop.sh  # Full round-trip testing
```

## Key Files

### Build Configuration
- `CMakeLists.txt` - Main build configuration
- `tests/CMakeLists.txt` - Test target definitions
- `CMakeModules/SetupTargetMacros.cmake` - Library/executable build macros
- `CMakeModules/FindSSE.cmake` - SSE detection

### Core Source Files
- `src/variant.cpp` - Dynamic type system (cornerstone of serialization)
- `src/exception.cpp` - Structured exception handling with stack traces
- `src/crypto/elliptic_common.cpp` - Common ECC code
- `src/crypto/elliptic_impl_priv.cpp` - secp256k1 ECC implementation
- `src/crypto/elliptic_impl_pub.cpp` - OpenSSL ECC implementation
- `src/io/json.cpp` - JSON serialization (uses simdjson)
- `src/thread/future.cpp` - Future/promise implementation

### Headers
- `include/fc/variant.hpp` - Variant type system
- `include/fc/api.hpp` - API framework
- `include/fc/reflect/reflect.hpp` - Reflection macros
- `include/fc/crypto/elliptic.hpp` - ECC interface

## Coding Conventions

### Naming
- Namespace: `fc::`
- Classes: lowercase (`variant`, `exception`, `thread`)
- Functions: snake_case (`to_variant`, `from_variant`)
- Member variables: underscore prefix (`_name`, `_what`)
- Constants/macros: UPPER_SNAKE_CASE

### Key Macros
- `FC_REFLECT(TYPE, MEMBERS)` - Enable reflection for types
- `FC_THROW_EXCEPTION(TYPE, MSG)` - Throw structured exception
- `FC_RETHROW_EXCEPTION(e, LOG_LEVEL, MSG)` - Rethrow with context
- `FC_UNUSED(...)` - Suppress unused variable warnings

### Serialization Pattern
```cpp
void to_variant(const MyType& obj, variant& v);
void from_variant(const variant& v, MyType& obj);
```

### Platform-Specific Code
```cpp
#ifdef _MSC_VER      // Visual Studio
#ifdef __GNUC__      // GCC/Clang
#ifdef __APPLE__     // macOS
#ifdef WIN32         // Windows
```

## CI/CD Notes

No `.gitlab-ci.yml` found in this repository. FC is typically built as a dependency of other projects (like Hive) which have their own CI pipelines.

### Build Targets
- `fc` - Static library with all features
- `fc_core` - Object library, core components
- `fc_shared_boost` - Static library with shared Boost linkage

### Library Dependencies
The final `fc` target links:
- `fc_core` (object library)
- `STATIC_BOOST` (Boost components)
- `STATIC_OPENSSL` (OpenSSL)
- `ZLIB`, `BZip2` (compression)
- Platform libs: `pthread`, `rt` (Linux), `wsock32`, `ws2_32` (Windows)

## Submodules

Initialize submodules before building:
```bash
git submodule update --init --recursive
```

Required submodules:
- `vendor/secp256k1-zkp` - ECC library
- `vendor/websocketpp` - WebSocket support
