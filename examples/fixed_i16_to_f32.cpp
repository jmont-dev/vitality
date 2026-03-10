#include <vitality/vitality.hpp>

#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <unordered_map>
#include <vector>

namespace {

struct StreamState {
    std::optional<vita::signal::format> format;
};

std::vector<float> decode_fixed_i16_iq_to_f32_interleaved(const vita::view::signal& sig,
                                                           const vita::signal::format& fmt) {
    const auto payload = sig.payload();
    if ((payload.size() % sizeof(std::int16_t)) != 0U) {
        throw std::runtime_error("payload size is not a multiple of 16-bit words");
    }

    const std::size_t word_count = payload.size() / sizeof(std::int16_t);
    if ((word_count % 2U) != 0U) {
        throw std::runtime_error("payload does not contain IQ pairs (odd 16-bit word count)");
    }

    std::vector<std::int16_t> iq_i16(word_count);
    std::memcpy(iq_i16.data(), payload.data(), payload.size());

    // Payload byte order conversion is application-defined.
    if (vita::host_is_little_endian()) {
        vita::byteswap_inplace(std::span<std::int16_t>(iq_i16));
    }

    const std::uint8_t frac_bits = fmt.data_item_fraction_size();
    const float scale = (frac_bits == 0U) ? 1.0f : 1.0f / static_cast<float>(1U << frac_bits);

    std::vector<float> iq_f32(word_count);
    for (std::size_t i = 0; i < word_count; ++i) {
        iq_f32[i] = static_cast<float>(iq_i16[i]) * scale;
    }

    return iq_f32;
}

} // namespace

int main() {
    constexpr std::uint32_t stream_id = 0x42424242u;

    // Context says payload is complex/cartesian signed fixed-point i16 with Q15 scaling.
    vita::signal::format format;
    format.set_real_complex_type(vita::RealComplexType::ComplexCartesian);
    format.set_data_item_format(vita::DataItemFormat::SignedFixedPoint);
    format.set_data_item_size(16u);
    format.set_data_item_fraction_size(15u);
    format.set_item_packing_field_size(16u);
    format.set_repeat_count(1u);
    format.set_vector_size(1u);

    vita::packet::context context_packet;
    context_packet.set_stream_id(stream_id);
    context_packet.set_change_indicator(true);
    context_packet.set_signal_data_format(format);
    const auto context_wire_bytes = context_packet.to_bytes();

    // Build big-endian payload bytes: [I0,Q0,I1,Q1] as 16-bit signed fixed-point.
    const std::vector<std::int16_t> tx_iq_i16 = {
        static_cast<std::int16_t>(16384),   // +0.5 in Q15
        static_cast<std::int16_t>(-8192),   // -0.25 in Q15
        static_cast<std::int16_t>(-32768),  // -1.0 in Q15
        static_cast<std::int16_t>(32767),   // ~+1.0 in Q15
    };

    std::vector<vita::byte> payload_be;
    payload_be.reserve(tx_iq_i16.size() * sizeof(std::int16_t));
    for (std::int16_t s : tx_iq_i16) {
        const auto u = static_cast<std::uint16_t>(s);
        payload_be.push_back(static_cast<vita::byte>((u >> 8U) & 0xFFU));
        payload_be.push_back(static_cast<vita::byte>(u & 0xFFU));
    }

    vita::packet::signal signal_packet;
    signal_packet.set_stream_id(stream_id);
    signal_packet.set_payload_view(vita::bytes_view(payload_be.data(), payload_be.size()));
    const auto signal_wire_bytes = signal_packet.to_bytes();

    std::unordered_map<std::uint32_t, StreamState> stream_state;

    const auto process_packet = [&](vita::bytes_view wire_bytes) {
        vita::packet::dispatch(
            wire_bytes,
            [&](const vita::view::signal& sig) {
                const auto sid = sig.stream_id().value_or(0u);
                const auto it = stream_state.find(sid);
                if (it == stream_state.end() || !it->second.format.has_value()) {
                    std::cout << "signal packet with no cached format yet\n";
                    return;
                }

                const auto& fmt = *it->second.format;
                if (fmt.real_complex_type() != vita::RealComplexType::ComplexCartesian ||
                    fmt.data_item_format() != vita::DataItemFormat::SignedFixedPoint ||
                    fmt.data_item_size() != 16u) {
                    std::cout << "unexpected signal format for this decoder\n";
                    return;
                }

                const auto iq_f32 = decode_fixed_i16_iq_to_f32_interleaved(sig, fmt);
                std::cout << "decoded float IQ (interleaved I,Q):\n";
                for (std::size_t i = 0; i < iq_f32.size(); i += 2) {
                    std::cout << "  I=" << iq_f32[i] << " Q=" << iq_f32[i + 1] << "\n";
                }
            },
            [&](const vita::view::context& ctx) {
                if (!ctx.has_signal_data_format() || !ctx.stream_id().has_value()) {
                    return;
                }
                stream_state[*ctx.stream_id()].format = ctx.signal_data_format();
                std::cout << "cached stream format for 0x" << std::hex << *ctx.stream_id() << std::dec << "\n";
            });
    };

    process_packet(vita::as_bytes_view(context_wire_bytes));
    process_packet(vita::as_bytes_view(signal_wire_bytes));

    return 0;
}
