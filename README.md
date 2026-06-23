# rude

`rude` is a C++23 Reliable UDP Engine built on Boost.Asio.

The project provides packet encoding, reliable/unreliable channel primitives,
sequence/ACK tracking, congestion-control policies, session configuration, and
testable transport abstractions. The endpoint/session layer is present, but the
stable, demonstrated surface today is the component layer.

## Current Status

Implemented and covered by tests:

- Default packet codec with CRC validation
- Reliable ordered channel delivery
- Reliable unordered channel delivery with duplicate suppression
- Unreliable sequenced delivery
- ACK bit tracking and sequence wraparound helpers
- Sliding-window and reorder-buffer helpers
- Token-bucket and BBR-lite congestion controllers
- Socket statistics snapshots
- CTest-integrated Boost.UT tests
- Component examples under `examples/`

Still early:

- Full connector/acceptor lifecycle
- Production retransmission scheduling integration
- Real loopback/session examples
- Public umbrella header completeness

## Requirements

- CMake 3.28 or newer
- A C++23-capable compiler
- vcpkg
- Boost.Asio, resolved through `vcpkg.json`
- Boost.UT for tests, resolved through the `tests` feature

On Windows/MSVC, run commands from a Visual Studio developer shell or initialize
the compiler environment first:

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"
```

## Build

Configure and build with tests and examples enabled:

```sh
cmake -S . -B build -DBOOST_UT_ENABLE_RUN_AFTER_BUILD=OFF
cmake --build build
```

If your vcpkg toolchain is not configured globally, pass it explicitly:

```sh
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DBOOST_UT_ENABLE_RUN_AFTER_BUILD=OFF
cmake --build build
```

Optional build flags:

```sh
-DRUDE_BUILD_TESTS=ON
-DRUDE_BUILD_EXAMPLES=ON
```

Both are enabled by default.

## Tests

Tests use Boost.UT and are registered with CTest when
`BOOST_UT_ENABLE_RUN_AFTER_BUILD=OFF`.

```sh
ctest --test-dir build --output-on-failure
```

Current test files:

- `tests/AckedSet.cpp`
- `tests/ChannelCommon.cpp`
- `tests/CodecDecode.cpp`
- `tests/Congestion.cpp`
- `tests/DefaultCodec.cpp`
- `tests/ReliableOrderedChannel.cpp`
- `tests/ReliableUnorderedChannel.cpp`
- `tests/ReorderBuffer.cpp`
- `tests/SendGuard.cpp`
- `tests/SessionConfig.cpp`
- `tests/SlidingWindow.cpp`
- `tests/SocketStats.cpp`
- `tests/UnreliableChannel.cpp`

Shared test helpers live in `tests/Helpers.hpp`.

## Examples

Examples are built as separate executables when `RUDE_BUILD_EXAMPLES=ON`.

- `CodecRoundTrip.cpp`: encodes and decodes a packet, showing the wire-format API.
- `ChannelDelivery.cpp`: demonstrates ordered, unordered, and unreliable delivery behavior.
- `CongestionControllers.cpp`: compares `Null`, `LeakyBucket`, and `BbrLite` control behavior.
- `SessionConfiguration.cpp`: shows session and channel configuration setup.
- `ErrorAndStats.cpp`: demonstrates error-code and statistics snapshot usage.

Example executables are generated as `rude_example_<Name>`.

```sh
./build/examples/rude_example_CodecRoundTrip
./build/examples/rude_example_ChannelDelivery
./build/examples/rude_example_CongestionControllers
./build/examples/rude_example_SessionConfiguration
./build/examples/rude_example_ErrorAndStats
```

On Windows:

```bat
build\examples\rude_example_CodecRoundTrip.exe
```

## Component Overview

### Codec

`rude::DefaultCodec` serializes `rude::Packet` into a compact binary frame:

- packet type
- sequence number
- latest ACK
- ACK bitfield
- channel ID
- payload
- CRC

Decode returns `std::expected<Packet, Error>`.

### Channels

Channel types model delivery semantics:

- `ReliableOrderedChannel`: buffers out-of-order packets and delivers in sequence.
- `ReliableUnorderedChannel`: delivers once, suppresses duplicate reliable packets.
- `UnreliableChannel`: delivers only newer sequenced packets.

### Congestion Control

Congestion policies satisfy `CongestionCtrl`:

- `detail::Null`: no pacing or limiting.
- `LeakyBucket`: token-bucket pacing for stable links.
- `BbrLite`: simplified BBR-style bandwidth/RTT estimator.

### Session Configuration

`SessionConfig` controls global session behavior such as MTU, keepalive, timeout,
and maximum channels.

`ChannelConfig` controls per-channel behavior:

- reliability mode
- ordering mode
- send window
- retransmit timeout
- priority

## Project Layout

- `src/rude/concepts`: C++ concepts for socket, codec, policy, congestion control
- `src/rude/core`: errors and executor aliases
- `src/rude/detail`: codec, socket wrapper, receive loop, buffers, queues, helpers
- `src/rude/detail/channel`: channel implementations
- `src/rude/detail/congestion`: congestion controllers
- `src/rude/endpoint`: connector and acceptor skeletons
- `src/rude/session`: session config, stats, and basic session type
- `examples`: component-focused examples
- `tests`: Boost.UT tests
- `cmake`: CMake helper modules

## Known Limitations

- `rude/rude.hpp` is currently only a placeholder umbrella header.
- Endpoint include paths currently contain case-sensitive inconsistencies on non-Windows filesystems.
- Full connector/acceptor examples are intentionally not provided yet because that lifecycle needs further hardening.
- The build may emit a Boost.Asio `_WIN32_WINNT` warning on Windows unless the target Windows version is defined by the consumer.

## License

MIT. See `LICENSE`.
