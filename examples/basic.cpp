#include <vitality/vitality.hpp>

#include <complex>
#include <cstring>
#include <iostream>
#include <vector>

int main() {
    std::vector<std::complex<float>> tx_samples = {
        {1.0f, -1.0f},
        {2.5f, 0.25f},
        {-3.0f, 4.0f},
    };

    vita::packet::signal packet;
    packet.set_stream_id(0x12345678u);
    packet.set_payload_view(vita::as_bytes_view(tx_samples));

    const auto wire_bytes = packet.to_bytes();
    const auto view = vita::view::signal::parse(vita::as_bytes_view(wire_bytes));

    std::vector<std::complex<float>> rx_samples(view.payload().size() / sizeof(std::complex<float>));
    std::memcpy(rx_samples.data(), view.payload().data(), view.payload().size());

    // Vitality handles VITA metadata fields as big-endian automatically.
    // Payload bytes are returned exactly as they arrived. If your payload word
    // order differs from the host representation, byteswap after copying.
    // Example:
    // if (vita::host_is_little_endian()) {
    //     vita::byteswap_inplace(std::span<std::complex<float>>(rx_samples));
    // }

    std::cout << "stream id: 0x" << std::hex << view.stream_id().value_or(0) << std::dec << "\n";
    for (const auto& sample : rx_samples) {
        std::cout << sample.real() << ", " << sample.imag() << "\n";
    }

    return 0;
}
