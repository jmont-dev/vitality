# Vitality

Vitality is a small header-only C++ library for working with common **VITA 49.2** signal and context packets.

It is designed around two goals:

- **Usability**: typed getters/setters for common fields instead of manual bit twiddling.
- **Performance**: packet parsing uses **views** over the original byte buffer so signal payloads are not copied unless you explicitly copy them.

## What it supports

Vitality currently supports the most common packet types used by SDR software:

- Signal Data Packet without Stream ID (`type 0`)
- Signal Data Packet with Stream ID (`type 1`)
- Context Packet (`type 4`)

### Supported common context fields (CIF0 subset)

Vitality parses and serializes this common CIF0 subset:

- Change indicator
- Reference point ID
- Bandwidth
- IF reference frequency
- RF reference frequency
- RF reference frequency offset
- IF band offset
- Reference level
- Gain (stage 1 / stage 2)
- Over-range count
- Sample rate
- Timestamp adjustment
- Timestamp calibration time
- Temperature
- Device identifier
- State/event indicators
- Signal data format

Packets that rely on unsupported CIF0/CIF1/CIF2/CIF3/CIF7 sections are rejected explicitly instead of being partially mis-parsed.

## Wire endianness

Vitality treats VITA packet metadata as **network-order / big-endian** on the wire and always converts those fields explicitly with byte-shifts.
That means:

- Header fields are decoded correctly on little-endian and big-endian hosts.
- Context fields like frequencies, sample rate, timestamps, and class IDs are normalized automatically.
- Serialization always writes canonical big-endian wire bytes.

For **signal payload bytes**, Vitality intentionally does **not** reinterpret or byte-swap sample data automatically.
The payload is exposed as a byte view because sample word order can depend on the stream's payload format and device conventions.
That keeps parsing zero-copy and avoids making assumptions about SDR sample layout.

## Public API

### Views

- `vita::view::signal`
- `vita::view::context`

These point into the original input buffer.

### Packets

- `vita::packet::signal`
- `vita::packet::context`
- `vita::packet::parse(...)`
- `vita::packet::parsed`

This structure makes the public entry points read as one family:

- build with `vita::packet::signal` / `vita::packet::context`
- parse with `vita::packet::parse(...)`
- inspect with `vita::view::signal` / `vita::view::context`

### Supporting types

- `vita::timestamp`
- `vita::class_id`
- `vita::packet_header`
- `vita::trailer`
- `vita::state_event_indicators`
- `vita::signal::format`

## Example

```cpp
#include <complex>
#include <cstring>
#include <vector>

#include <vitality/vitality.hpp>

int main() {
    std::vector<std::complex<float>> tx_samples = {
        {1.0f, -1.0f},
        {2.5f, 0.25f},
    };

    vita::packet::signal packet;
    packet.set_stream_id(0x12345678u);
    packet.set_payload_view(vita::as_bytes_view(tx_samples));

    auto wire_bytes = packet.to_bytes();
    auto view = vita::view::signal::parse(vita::as_bytes_view(wire_bytes));

    std::vector<std::complex<float>> rx_samples(view.payload().size() / sizeof(std::complex<float>));
    std::memcpy(rx_samples.data(), view.payload().data(), view.payload().size());

    // Swap payload words only if your payload convention requires it.
    // vita::byteswap_inplace(std::span<std::complex<float>>(rx_samples));
}
```

## Build

Vitality is header-only for library consumers, and the repository also includes a small unit test suite built with the vendored single-header doctest framework. There are two examples:

- `examples/basic.cpp` for a minimal in-memory signal-packet round trip
- `examples/socket.cpp` for a simple localhost UDP send / `recvfrom` / parse flow

```cpp
#include <vitality/vitality.hpp>
```

or, if you want the single-file drop-in form:

```cpp
#include "Vitality.hpp"
```

Compile with a C++20 compiler because the library uses `std::span`.

## Notes on scope

VITA 49.2 is intentionally very flexible and many real SDR deployments use specialized subsets.
Vitality focuses on the common signal/context path and keeps unsupported advanced sections explicit.
That keeps the API small, predictable, and fast for the common case.

## Tests

Configure and build as usual with CMake, then run the test target:

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The current test suite covers:

- signal-data serialize/parse round-trips
- context serialize/parse round-trips for the supported CIF0 subset
- packet-type dispatch through `vita::packet::parse(...)`
- big-endian wire encoding for metadata fields
- localhost UDP socket send/receive integration for context and complex-float signal packets
- byte-swap helpers for common payload types
- error handling for malformed packet sizes, unsupported indicators, missing required fields, and invalid serialization inputs
