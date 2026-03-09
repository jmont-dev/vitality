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

    vita::signal::packet packet;
    packet.set_stream_id(0x12345678u);
    packet.set_payload_view(vita::as_bytes_view(tx_samples));

    const std::vector<vita::byte> wire_bytes = packet.to_bytes();
    const auto view = vita::signal::view::parse(vita::as_bytes_view(wire_bytes));

    std::vector<std::complex<float>> rx_samples(view.payload().size() / sizeof(std::complex<float>));
    std::memcpy(rx_samples.data(), view.payload().data(), view.payload().size());

    // VITA metadata words are handled as big-endian by Vitality.
    // Payload bytes are exposed exactly as received. If the payload word order
    // on the wire differs from your host representation, byteswap after copying.
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
