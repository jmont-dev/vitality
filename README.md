# Vitality - Header-only VITA 49.2 support in C++

Vitality is a small header-only C++20-26 library for working with signal and context packets from the **VITA 49.2** radio standard. This standard defines common wire formats for interfacing with software-defined radios.

It is designed around two goals:

- **Usability**: typed getters and setters for common fields instead of manual bit twiddling.
- **Performance**: parsing uses **views** over the original byte buffer so payloads are not copied unless you explicitly copy them.

## Example usage

### Create a signal packet
```C++
    // Define a vector to hold IQ samples
    std::vector<std::complex<float>> tx_samples = {
        {1.0f, -1.0f},
        {2.5f, 0.25f},
    };

    // Create a signal packet and assign the payload
    vita::packet::signal signal_packet;
    signal_packet.set_stream_id(0x12345678u);
    signal_packet.set_payload_view(vita::as_bytes_view(tx_samples));
    
    // Send these bytes on your socket
    const auto signal_wire_bytes = signal_packet.to_bytes();
```
### Create a context packet
```C++
    // Create a context packet and set metadata fields
    vita::packet::context context_packet;
    context_packet.set_stream_id(0xABCDEF01u);
    context_packet.set_change_indicator(true);
    context_packet.set_sample_rate_sps(30.72e6);
    context_packet.set_temperature_celsius(41.5);

    // Send these bytes on your socket
    const auto context_wire_bytes = context_packet.to_bytes();
```

### Handle signal or context packets
```C++

    struct PacketHandler {
        void on_signal(const vita::view::signal& view) const {
            std::vector<std::complex<float>> samples(view.payload().size() / sizeof(std::complex<float>));
            std::memcpy(samples.data(), view.payload().data(), view.payload().size());

            std::cout << "signal stream id: 0x" << std::hex << view.stream_id().value_or(0u) << std::dec << "\n";
            for (const auto& sample : samples) {
                std::cout << "  " << sample.real() << ", " << sample.imag() << "\n";
            }
        }

        void on_context(const vita::view::context& view) const {
            std::cout << "context stream id: 0x" << std::hex << view.stream_id().value_or(0u) << std::dec
                    << ", sample-rate=" << (view.has_sample_rate_sps() ? "present" : "missing")
                    << ", temperature=" << (view.has_temperature_celsius() ? "present" : "missing") << "\n";
        }
    };

    PacketHandler handler;
    auto signal_handler = std::bind(&PacketHandler::on_signal, &handler, std::placeholders::_1);
    auto context_handler = std::bind(&PacketHandler::on_context, &handler, std::placeholders::_1);

    std::vector<vita::byte> recv_buffer(2048);
    const auto received = ::recv(rx_fd, recv_buffer.data(), recv_buffer.size(), 0);
    if (received < 0) {
        throw std::system_error(errno, std::generic_category(), "recv");
    }
    recv_buffer.resize(static_cast<std::size_t>(received));

    vita::packet::dispatch(vita::as_bytes_view(recv_buffer), signal_handler, context_handler);

```

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

## Dispatch example

`examples/dispatch.cpp` shows how to bind your own handlers for both packet kinds and delegate parse+visit in one call:

- build one signal packet and one context packet
- send both over localhost UDP
- bind member functions with `std::bind`
- call `vita::packet::dispatch(...)` so the matching handler runs automatically

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
