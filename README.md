# Vitality

Vitality is a small header-only C++ library for working with common **VITA 49.2** signal-data and context packets.

It is designed around two goals:

- **Usability**: typed getters and setters for common fields instead of manual bit twiddling.
- **Performance**: parsing uses **views** over the original byte buffer so payloads are not copied unless you explicitly copy them.

## Scope

Vitality focuses on the common SDR path:

- signal-data packets (`packet type 0` and `1`)
- standard context packets (`packet type 4`)
- a practical common CIF0 subset for context metadata

Unsupported CIF sections are rejected explicitly instead of being partially mis-parsed.

## Public API

### Packet builders

- `vita::packet::signal`
- `vita::packet::context`
- `vita::packet::parse(...)`

### Parse views

- `vita::view::signal`
- `vita::view::context`

### Common supporting types

- `vita::byte`
- `vita::bytes_view`
- `vita::timestamp`
- `vita::class_id`
- `vita::packet_header`
- `vita::trailer`
- `vita::state_event_indicators`
- `vita::signal::format`

## Supported context fields

The current context implementation supports this common CIF0 subset:

- change indicator
- reference point ID
- bandwidth
- IF reference frequency
- RF reference frequency
- RF reference frequency offset
- IF band offset
- reference level
- gain (stage 1 and stage 2)
- over-range count
- sample rate
- timestamp adjustment
- timestamp calibration time
- temperature
- device identifier
- state and event indicators
- signal-data format

## Endianness

Vitality treats VITA metadata as **big-endian** on the wire and converts those fields explicitly during parse and serialization.

That means:

- headers, timestamps, class IDs, and context fields are normalized automatically
- packets are serialized back to canonical big-endian wire bytes

Payload bytes are intentionally exposed exactly as received.
Vitality does **not** reinterpret or byte-swap payload samples automatically because payload layout is stream-specific.

For common sample payloads you can use the built-in helpers:

- `vita::byteswap16(...)`
- `vita::byteswap32(...)`
- `vita::byteswap64(...)`
- `vita::byteswap(...)`
- `vita::byteswap_inplace(...)`
- `vita::host_is_little_endian()`
- `vita::host_is_big_endian()`

Example for float32 IQ payloads after copying out of a payload view:

```cpp
std::vector<std::complex<float>> samples(...);
std::memcpy(samples.data(), view.payload().data(), view.payload().size());

if (vita::host_is_little_endian()) {
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
}
```

Use that only when your wire convention stores payload words in the opposite byte order from your host.

## Basic example

`examples/basic.cpp` shows the smallest in-memory round trip:

- start with `std::vector<std::complex<float>>`
- build a `vita::packet::signal`
- serialize to bytes
- parse with `vita::view::signal`
- copy payload bytes back into typed samples

## Socket example

`examples/socket.cpp` shows the simplest real-world path:

- start with `std::vector<std::complex<float>>`
- build a `vita::packet::signal`
- send with `sendto(...)`
- receive with `recvfrom(...)`
- parse with `vita::view::signal`

## Build

Include the namespaced header:

```cpp
#include <vitality/vitality.hpp>
```

or use the single-file drop-in form:

```cpp
#include "Vitality.hpp"
```

A C++20 compiler is required because the library uses `std::span` and `std::bit_cast`.

## Tests

The repository includes a self-contained doctest suite.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The suite covers:

- signal-data serialize/parse round trips
- context serialize/parse round trips for the supported CIF0 subset
- packet dispatch through `vita::packet::parse(...)`
- byte-swap helpers for common payload types
- malformed packet and unsupported-indicator handling
- localhost UDP integration for context and signal packets
