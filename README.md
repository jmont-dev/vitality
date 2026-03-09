# Vitality

Vitality is a small header-only C++ library for working with common **VITA 49.2** signal data and context packets.

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

Vitality treats VITA packet metadata as **network-order / big-endian** on the wire and always converts those fields explicitly with byte shifts. That means:

- header fields are decoded correctly on little-endian and big-endian hosts
- context fields like frequencies, sample rate, timestamps, and class IDs are normalized automatically
- serialization always writes canonical big-endian wire bytes

For **signal payload bytes**, Vitality intentionally does **not** reinterpret or byte-swap sample data automatically. The payload is exposed as a byte view because sample word order can depend on the stream's payload format and device conventions. That keeps parsing zero-copy and avoids making assumptions about SDR sample layout.

To help with the common cases, the library now includes byte-swap helpers directly in the public API:

- `vitality::byteswap16(...)`
- `vitality::byteswap32(...)`
- `vitality::byteswap64(...)`
- `vitality::byteswap(...)`
- `vitality::byteswap_inplace(...)`
- `vitality::host_is_little_endian()`
- `vitality::host_is_big_endian()`

The generic `byteswap(...)` and `byteswap_inplace(...)` helpers support the standard payload types users most often run into in VITA streams:

- `std::uint16_t` / `std::int16_t`
- `std::uint32_t` / `std::int32_t`
- `std::uint64_t` / `std::int64_t`
- `float`
- `double`
- `std::complex<float>`
- `std::complex<double>`

Example:

```cpp
std::vector<std::complex<float>> iq = {{1.0f, -1.0f}, {0.5f, 0.25f}};

if (vitality::host_is_little_endian()) {
    vitality::byteswap_inplace(std::span<std::complex<float>>{iq});
}
```

Apply those helpers only when your payload word order actually differs from your host byte order.

## Main types

### Parsing views

- `vitality::SignalDataPacketView`
- `vitality::ContextPacketView`
- `vitality::ParsedPacket`
- `vitality::parse_packet(...)`

These views point into the original input buffer.

### Builders / owning packet types

- `vitality::SignalDataPacket`
- `vitality::ContextPacket`

These own or reference the data you want to serialize back onto the wire.

## Byte views for typed payloads

For payloads such as `std::vector<std::complex<float>>`, Vitality provides convenient helpers:

- `vitality::as_bytes_view(std::vector<T>)`
- `vitality::as_bytes_view(std::span<const T>)`
- `vitality::as_writable_bytes_view(std::vector<T>)`
- `vitality::as_writable_bytes_view(std::span<T>)`

That makes it easy to move between typed sample buffers and packet payload views without manual pointer casts.

## Example

```cpp
#include <complex>
#include <iostream>
#include <vector>

#include "vitality/vitality.hpp"

int main() {
    using namespace vitality;

    std::vector<std::complex<float>> iq = {
        {1.0f, -1.0f},
        {0.5f, 0.25f},
    };

    SignalDataPacket packet;
    packet.set_stream_id(0xDEADBEEFu);
    packet.set_payload_view(as_bytes_view(iq));

    auto wire_bytes = packet.to_bytes();
    auto parsed = SignalDataPacketView::parse(as_bytes_view(wire_bytes));

    std::vector<std::complex<float>> recovered(iq.size());
    std::memcpy(recovered.data(), parsed.payload().data(), parsed.payload().size());

    std::cout << "stream id = 0x" << std::hex << parsed.stream_id().value() << std::dec << "\n";
    std::cout << "first I sample = " << recovered.front().real() << "\n";
}
```

## Build

Vitality is header-only for library consumers, and the repository also includes a small unit test suite built with the vendored single-header doctest framework. There are two examples:

- `examples/basic.cpp` for in-memory packet round-tripping
- `examples/socket.cpp` for the minimal `std::vector<std::complex<float>>` -> UDP `sendto` -> `recvfrom` -> `SignalDataPacketView` flow

```cpp
#include "vitality/vitality.hpp"
```

or, if you want the single-file drop-in form:

```cpp
#include "Vitality.hpp"
```

Compile with a C++20 compiler because the library uses `std::span` and `std::bit_cast`.

## Notes on scope

VITA 49.2 is intentionally very flexible and many real SDR deployments use specialized subsets. Vitality focuses on the common signal/context path and keeps unsupported advanced sections explicit. That keeps the API small, predictable, and fast for the common case.

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
- packet-type dispatch through `parse_packet(...)`
- byte-swap helpers for common payload types
- localhost UDP socket send/receive integration for context and complex-float signal packets
- error handling for malformed packet sizes, unsupported indicators, missing required fields, and invalid serialization inputs
