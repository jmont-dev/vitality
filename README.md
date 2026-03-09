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

Vitality treats VITA packet metadata as **network-order / big-endian** on the wire and always converts those fields explicitly with byte-shifts.
That means:

- Header fields are decoded correctly on little-endian and big-endian hosts.
- Context fields like frequencies, sample rate, timestamps, and class IDs are normalized automatically.
- Serialization always writes canonical big-endian wire bytes.

For **signal payload bytes**, Vitality intentionally does **not** reinterpret or byte-swap sample data automatically.
The payload is exposed as a byte view because sample word order can depend on the stream's payload format and device conventions.
That keeps parsing zero-copy and avoids making assumptions about SDR sample layout.

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

## Example

```cpp
#include <iostream>
#include <vector>

#include "vitality/vitality.hpp"

int main() {
    using namespace vitality;

    ContextPacket ctx;
    ctx.set_stream_id(0xDEADBEEFu);
    ctx.set_change_indicator(true);
    ctx.set_bandwidth_hz(8.0e6);
    ctx.set_rf_reference_frequency_hz(100.0e6);
    ctx.set_sample_rate_sps(10.0e6);

    SignalDataFormat fmt;
    fmt.set_packing_method(PackingMethod::ProcessingEfficient);
    fmt.set_real_complex_type(RealComplexType::ComplexCartesian);
    fmt.set_data_item_format(DataItemFormat::SignedFixedPoint);
    fmt.set_data_item_size(16);
    fmt.set_item_packing_field_size(16);
    ctx.set_signal_data_format(fmt);

    auto ctx_bytes = ctx.to_bytes();
    auto parsed_ctx = std::get<ContextPacketView>(parse_packet(as_bytes_view(ctx_bytes)));

    std::cout << parsed_ctx.rf_reference_frequency_hz() << "\n";

    std::vector<byte> iq(16);
    SignalDataPacket sig;
    sig.set_stream_id(0xDEADBEEFu);
    sig.set_payload_view(bytes_view{iq.data(), iq.size()});

    auto sig_bytes = sig.to_bytes();
    auto parsed_sig = std::get<SignalDataPacketView>(parse_packet(as_bytes_view(sig_bytes)));
    std::cout << parsed_sig.payload().size() << "\n";
}
```

## Build

Vitality is header-only for library consumers, and the repository also includes a small unit test suite built with the vendored single-header doctest framework. There are two examples: `examples/basic.cpp` for in-memory round-tripping and `examples/socket.cpp` for sending VITA packets over a localhost UDP socket and parsing them on receipt.

```cpp
#include "vitality/vitality.hpp"
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
- packet-type dispatch through `parse_packet(...)`
- big-endian wire encoding for metadata fields
- localhost UDP socket send/receive integration for context and signal packets
- error handling for malformed packet sizes, unsupported indicators, missing required fields, and invalid serialization inputs
