#include <vitality/vitality.hpp>

#include <complex>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

// Track stream-specific metadata. In VITA systems, stream configuration is
// typically delivered in context packets and then applied to later signal data.
struct StreamState {
    std::optional<vita::signal::format> format;
};

const char* to_string(vita::RealComplexType value) {
    switch (value) {
        case vita::RealComplexType::Real:
            return "real";
        case vita::RealComplexType::ComplexCartesian:
            return "complex/cartesian";
        case vita::RealComplexType::ComplexPolar:
            return "complex/polar";
        case vita::RealComplexType::Reserved:
        default:
            return "reserved";
    }
}

const char* to_string(vita::DataItemFormat value) {
    switch (value) {
        case vita::DataItemFormat::SignedFixedPoint:
            return "signed fixed-point";
        case vita::DataItemFormat::UnsignedFixedPoint:
            return "unsigned fixed-point";
        case vita::DataItemFormat::IEEE754Single:
            return "IEEE754 float32";
        case vita::DataItemFormat::IEEE754Double:
            return "IEEE754 float64";
        default:
            return "other/stream-specific";
    }
}

void print_format_summary(const vita::signal::format& fmt) {
    std::cout << "  format:\n";
    std::cout << "    real/complex      : " << to_string(fmt.real_complex_type()) << "\n";
    std::cout << "    item format       : " << to_string(fmt.data_item_format()) << "\n";
    std::cout << "    data item size    : " << static_cast<int>(fmt.data_item_size()) << " bits\n";
    std::cout << "    packing field size: " << static_cast<int>(fmt.item_packing_field_size()) << " bits\n";
    std::cout << "    repeat count      : " << fmt.repeat_count() << "\n";
    std::cout << "    vector size       : " << fmt.vector_size() << "\n";
}

} // namespace

int main() {
    // Use one stream ID for both packets to model a real receiver.
    constexpr std::uint32_t stream_id = 0x10203040u;

    // 1) Build a context packet that declares how signal payload samples are laid out.
    vita::signal::format format;
    format.set_real_complex_type(vita::RealComplexType::ComplexCartesian);
    format.set_data_item_format(vita::DataItemFormat::IEEE754Single); // float32
    format.set_data_item_size(32u);
    format.set_item_packing_field_size(32u);
    format.set_repeat_count(1u);
    format.set_vector_size(1u);

    vita::packet::context context_packet;
    context_packet.set_stream_id(stream_id);
    context_packet.set_change_indicator(true);
    context_packet.set_signal_data_format(format);
    const auto context_wire_bytes = context_packet.to_bytes();

    // 2) Build a signal packet carrying actual IQ payload bytes.
    std::vector<std::complex<float>> tx_iq = {
        {1.0f, -1.0f},
        {2.5f, 0.25f},
        {-3.0f, 4.0f},
    };

    vita::packet::signal signal_packet;
    signal_packet.set_stream_id(stream_id);
    signal_packet.set_payload_view(vita::as_bytes_view(tx_iq));
    const auto signal_wire_bytes = signal_packet.to_bytes();

    // 3) Receiver state keyed by stream ID. Cache context-derived format data here.
    std::unordered_map<std::uint32_t, StreamState> stream_state;

    // Helper to parse either packet type and do the right thing.
    const auto process_packet = [&](vita::bytes_view wire_bytes) {
        vita::packet::dispatch(
            wire_bytes,
            [&](const vita::view::signal& sig) {
                const auto sid = sig.stream_id().value_or(0u);
                std::cout << "signal packet: stream_id=0x" << std::hex << sid << std::dec << "\n";

                // You generally cannot infer payload sample type from the signal packet alone.
                // Look up the latest context for this stream.
                const auto it = stream_state.find(sid);
                if (it == stream_state.end() || !it->second.format.has_value()) {
                    std::cout << "  no signal-data format cached yet; defer decoding\n";
                    return;
                }

                const auto& fmt = *it->second.format;
                print_format_summary(fmt);

                // Decode only when format matches what we expect in this demo.
                if (fmt.real_complex_type() == vita::RealComplexType::ComplexCartesian &&
                    fmt.data_item_format() == vita::DataItemFormat::IEEE754Single &&
                    fmt.data_item_size() == 32u) {
                    std::vector<std::complex<float>> rx_iq(sig.payload().size() / sizeof(std::complex<float>));
                    std::memcpy(rx_iq.data(), sig.payload().data(), sig.payload().size());

                    // If your wire convention uses opposite byte order for payload words,
                    // byteswap after copying. The library does not byteswap payloads.
                    // if (vita::host_is_little_endian()) {
                    //     vita::byteswap_inplace(std::span<std::complex<float>>(rx_iq));
                    // }

                    std::cout << "  decoded IQ samples:\n";
                    for (const auto& sample : rx_iq) {
                        std::cout << "    " << sample.real() << ", " << sample.imag() << "\n";
                    }
                } else {
                    std::cout << "  unsupported format for this demo; add decode path\n";
                }
            },
            [&](const vita::view::context& ctx) {
                const auto sid = ctx.stream_id().value_or(0u);
                std::cout << "context packet: stream_id=0x" << std::hex << sid << std::dec << "\n";

                if (!ctx.has_signal_data_format()) {
                    std::cout << "  context did not include signal-data format\n";
                    return;
                }

                auto& state = stream_state[sid];
                state.format = ctx.signal_data_format();
                std::cout << "  cached signal-data format for stream\n";
                print_format_summary(*state.format);
            });
    };

    // In real systems packets may arrive in any order.
    // First show what happens if signal arrives before context.
    process_packet(vita::as_bytes_view(signal_wire_bytes));

    // Then process context, which enables decoding of subsequent signal packets.
    process_packet(vita::as_bytes_view(context_wire_bytes));
    process_packet(vita::as_bytes_view(signal_wire_bytes));

    return 0;
}
