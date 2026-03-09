# Vitality

Vitality is a header-only C++ library for turning common VITA 49.2 signal and context packets into easy in-memory objects and views, and for building those packets back into wire bytes.

The public API is organized under `vita::...`.

## Public API

```cpp
vita::signal::packet
vita::signal::view
vita::context::packet
vita::context::view
vita::packet::parse(...)
```

Useful supporting types are also available at the top level:

```cpp
vita::timestamp
vita::class_id
vita::packet_header
vita::trailer
vita::signal::format
vita::signal_data_format
vita::state_event_indicators
```

## Design goals

- header-only
- zero-copy parsing for packet input
- explicit packet builders for serialization
- clear handling of big-endian VITA metadata
- no automatic payload byte-swapping

Vitality always decodes and encodes VITA metadata words as big-endian. Payload bytes are exposed as a view exactly as they appear on the wire. That keeps parsing fast and avoids guessing how your sample payload should be interpreted.

## Payload byte-swapping

Common payload helpers are built in:

```cpp
vita::byteswap16(...)
vita::byteswap32(...)
vita::byteswap64(...)
vita::byteswap(...)
vita::byteswap_inplace(...)
```

Supported common payload types include:

- 16/32/64-bit signed and unsigned integers
- `float`
- `double`
- `std::complex<float>`
- `std::complex<double>`

Example:

```cpp
std::vector<std::complex<float>> samples = ...;

if (vita::host_is_little_endian()) {
    vita::byteswap_inplace(std::span<std::complex<float>>(samples));
}
```

Only do this when the payload word order on the wire differs from the host representation you want to use in memory.

## Minimal signal packet example

```cpp
std::vector<std::complex<float>> samples = {
    {1.0f, -1.0f},
    {2.0f, -2.0f},
};

vita::signal::packet packet;
packet.set_stream_id(0x12345678u);
packet.set_payload_view(vita::as_bytes_view(samples));

const auto wire = packet.to_bytes();
const auto view = vita::signal::view::parse(vita::as_bytes_view(wire));
```

See `examples/basic.cpp` for the smallest in-memory round-trip example, and `examples/socket.cpp` for the POSIX UDP version:

- start with `std::vector<std::complex<float>>`
- build a `vita::signal::packet`
- `sendto(...)`
- `recvfrom(...)`
- parse with `vita::signal::view`
- copy payload bytes back into `std::vector<std::complex<float>>`

## Build

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

This builds:

- `vitality_tests`
- `vitality_basic_example`

On Unix-like systems it also builds:

- `vitality_socket_example`

## Test coverage

The doctest suite covers:

- signal packet round-trips
- context packet round-trips
- packet dispatch through `vita::packet::parse(...)`
- big-endian metadata serialization
- byte-swap helpers
- localhost UDP send/receive integration for context and signal packets
