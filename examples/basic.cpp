#include <cstddef>
#include <cstdint>
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

    std::cout << "context stream id = 0x" << std::hex << parsed_ctx.stream_id().value() << std::dec << "\n";
    std::cout << "rf center = " << parsed_ctx.rf_reference_frequency_hz() << " Hz\n";
    std::cout << "sample rate = " << parsed_ctx.sample_rate_sps() << " sps\n";

    std::vector<byte> iq(16);
    for (std::size_t i = 0; i < iq.size(); ++i) {
        iq[i] = static_cast<byte>(i);
    }

    SignalDataPacket sig;
    sig.set_stream_id(0xDEADBEEFu);
    sig.set_payload_view(bytes_view{iq.data(), iq.size()});
    auto sig_bytes = sig.to_bytes();

    auto parsed_sig = std::get<SignalDataPacketView>(parse_packet(as_bytes_view(sig_bytes)));
    std::cout << "signal payload bytes = " << parsed_sig.payload().size() << "\n";
}
